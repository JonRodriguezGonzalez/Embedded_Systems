/*
 * Fase 2 - Monitor de nivel con maquina de estados.
 *
 * Lee un potenciometro (ADC1_CH0 / GPIO1), filtra el ruido con una media
 * movil, y gobierna una alarma sonora intermitente (buzzer pasivo en GPIO4
 * via PWM LEDC) mediante una maquina de estados con histeresis. Incluye
 * deteccion de fallos de lectura (estado FAULT con salida segura) y un
 * watchdog hardware (TWDT) que reinicia el sistema si el bucle se cuelga.
 *
 * Hardware: ESP32-S3.
 *   - Potenciometro: 3V3 / GPIO1 / GND (alimentar a 3V3, NO a 5V).
 *   - Buzzer pasivo: GPIO4 / GND.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

static const char *TAG = "monitor";

/* --- ADC --- */
#define POT_ADC_UNIT     ADC_UNIT_1
#define POT_ADC_CHANNEL  ADC_CHANNEL_0
#define POT_ADC_ATTEN    ADC_ATTEN_DB_12
#define POT_ADC_BITWIDTH ADC_BITWIDTH_12

/* --- Buzzer (LEDC / PWM) --- */
#define BUZZER_GPIO      4
#define BUZZER_TIMER     LEDC_TIMER_0
#define BUZZER_MODE      LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL   LEDC_CHANNEL_0
#define BUZZER_RES       LEDC_TIMER_10_BIT
#define BUZZER_FREQ_HZ   2000
#define BUZZER_DUTY_ON   512

/* --- Muestreo / filtro --- */
#define SAMPLE_PERIOD_MS 100
#define FILTER_WINDOW    8

/* --- Histeresis (cuentas de ADC) --- */
#define THRESH_HIGH      2800
#define THRESH_LOW       1200

/* --- Alarma intermitente --- */
#define BEEP_PERIOD_MS   200

/* --- Deteccion de fallos --- */
#define MAX_CONSEC_FAILS 5

/* --- Watchdog --- */
#define WDT_TIMEOUT_S    3

/* --- Estados --- */
typedef enum {
    ST_INIT = 0,
    ST_NORMAL_LOW,
    ST_NORMAL_HIGH,
    ST_FAULT,
} system_state_t;

static const char *state_name(system_state_t s)
{
    switch (s) {
        case ST_INIT:        return "INIT";
        case ST_NORMAL_LOW:  return "NORMAL_LOW";
        case ST_NORMAL_HIGH: return "NORMAL_HIGH";
        case ST_FAULT:       return "FAULT";
        default:             return "?";
    }
}

/* --- Filtro media movil (O(1) por muestra mediante suma acumulada) --- */
static int  s_window[FILTER_WINDOW];
static int  s_window_idx = 0;
static int  s_window_count = 0;
static long s_window_sum = 0;

static int filter_update(int sample)
{
    if (s_window_count == FILTER_WINDOW) {
        s_window_sum -= s_window[s_window_idx];
    } else {
        s_window_count++;
    }
    s_window[s_window_idx] = sample;
    s_window_sum += sample;
    s_window_idx = (s_window_idx + 1) % FILTER_WINDOW;
    return (int)(s_window_sum / s_window_count);
}

static void filter_reset(void)
{
    s_window_idx = 0;
    s_window_count = 0;
    s_window_sum = 0;
}

/* --- Buzzer --- */
static void buzzer_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = BUZZER_MODE,
        .timer_num       = BUZZER_TIMER,
        .duty_resolution = BUZZER_RES,
        .freq_hz         = BUZZER_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .speed_mode = BUZZER_MODE,
        .channel    = BUZZER_CHANNEL,
        .timer_sel  = BUZZER_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = BUZZER_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

static void buzzer_set(bool on)
{
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, on ? BUZZER_DUTY_ON : 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void app_main(void)
{
    /* Init ADC */
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = POT_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = POT_ADC_ATTEN,
        .bitwidth = POT_ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, POT_ADC_CHANNEL, &chan_cfg));

    /* Init buzzer */
    buzzer_init();

    /* Init watchdog sobre la tarea del bucle principal */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t wdt_err = esp_task_wdt_init(&wdt_cfg);
    if (wdt_err == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&wdt_cfg);
    }
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    system_state_t state = ST_INIT;
    ESP_LOGI(TAG, "Iniciando. THRESH_LOW=%d THRESH_HIGH=%d WDT=%ds",
             THRESH_LOW, THRESH_HIGH, WDT_TIMEOUT_S);

    bool    beep_on = false;
    int64_t last_beep_toggle = now_ms();
    int     consec_fails = 0;

    state = ST_NORMAL_LOW;
    buzzer_set(false);

    while (1) {
        esp_task_wdt_reset();   /* alimentar el watchdog */

        int raw = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &raw);

        system_state_t prev = state;

        if (err == ESP_OK) {
            consec_fails = 0;

            if (state == ST_FAULT) {
                filter_reset();
                state = ST_NORMAL_LOW;
            }

            int filtered = filter_update(raw);

            switch (state) {
                case ST_NORMAL_LOW:
                    if (filtered > THRESH_HIGH) state = ST_NORMAL_HIGH;
                    break;
                case ST_NORMAL_HIGH:
                    if (filtered < THRESH_LOW) state = ST_NORMAL_LOW;
                    break;
                default:
                    state = ST_NORMAL_LOW;
                    break;
            }

            if (state == ST_NORMAL_HIGH) {
                if (now_ms() - last_beep_toggle >= BEEP_PERIOD_MS) {
                    beep_on = !beep_on;
                    buzzer_set(beep_on);
                    last_beep_toggle = now_ms();
                }
            } else {
                if (beep_on) { beep_on = false; buzzer_set(false); }
            }

            if (state != prev) {
                ESP_LOGW(TAG, ">>> %s -> %s (filt=%d)",
                         state_name(prev), state_name(state), filtered);
            } else {
                ESP_LOGI(TAG, "raw=%4d filt=%4d estado=%s",
                         raw, filtered, state_name(state));
            }
        } else {
            consec_fails++;
            ESP_LOGW(TAG, "Lectura fallida (%d/%d): %s",
                     consec_fails, MAX_CONSEC_FAILS, esp_err_to_name(err));

            if (consec_fails >= MAX_CONSEC_FAILS && state != ST_FAULT) {
                state = ST_FAULT;
                beep_on = false;
                buzzer_set(false);
                ESP_LOGE(TAG, ">>> %s -> FAULT (salida segura)", state_name(prev));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
