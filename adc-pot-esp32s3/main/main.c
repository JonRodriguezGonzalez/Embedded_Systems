/*
 * adc-pot-esp32s3 — Lectura analógica de un potenciometro vía ADC1.
 *
 * Lee periodicamente el cursor de un potenciometro (divisor de tension)
 * conectado a GPIO1 (ADC1_CH0) e imprime el valor crudo y una estimacion
 * en milivoltios.
 *
 * Hardware: ESP32-S3. Alimentar el potenciometro a 3V3 (NO a 5V).
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "adc_pot";

/* --- Configuracion del ADC (ver docs/requisitos.md REQ-FUN-01, REQ-HW-02) --- */
#define POT_ADC_UNIT     ADC_UNIT_1       /* ADC1: ADC2 colisiona con el WiFi */
#define POT_ADC_CHANNEL  ADC_CHANNEL_0    /* GPIO1 en ESP32-S3 = ADC1 canal 0 */
#define POT_ADC_ATTEN    ADC_ATTEN_DB_12  /* rango de entrada ~0..3.3 V       */
#define POT_ADC_BITWIDTH ADC_BITWIDTH_12  /* resolucion 12 bits -> 0..4095    */

#define SAMPLE_PERIOD_MS 500              /* periodo de muestreo (REQ-FUN-02) */
#define ADC_VREF_MV      3300             /* fondo de escala nominal          */
#define ADC_MAX_COUNTS   4095             /* 2^12 - 1                         */

void app_main(void)
{
    /* 1. Inicializar la unidad ADC */
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = POT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    /* 2. Configurar el canal */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = POT_ADC_ATTEN,
        .bitwidth = POT_ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, POT_ADC_CHANNEL, &chan_cfg));

    ESP_LOGI(TAG, "ADC inicializado. Gira el potenciometro.");

    /* 3. Bucle de muestreo */
    while (1) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &raw);

        if (err == ESP_OK) {
            /* Conversion ingenua a mV (ver docs/notas-tecnicas.md seccion 4) */
            int mv_approx = (raw * ADC_VREF_MV) / ADC_MAX_COUNTS;
            ESP_LOGI(TAG, "raw=%4d  ~%4d mV", raw, mv_approx);
        } else {
            /* REQ-ROB-02: ante fallo, avisar y seguir operando */
            ESP_LOGW(TAG, "Lectura fallida: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
