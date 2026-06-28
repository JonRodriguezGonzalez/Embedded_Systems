/*
 * Fase 3 - Emisor (ESP32-S3): protocolo UART fiable.
 *
 * Envia tramas con numero de secuencia y CRC16, espera ACK, y retransmite
 * por timeout si no llega. Demuestra entrega fiable sobre un canal serie.
 *
 * Trama de datos: [SOF][SEQ][LEN][PAYLOAD...][CRC_hi][CRC_lo][EOF]
 * Trama de ACK  : [ACK_SOF][SEQ][ACK_EOF]
 *
 * Cableado (3.3V, directo, sin convertidor de niveles):
 *   ESP32 GPIO17 (TX) -> Pi GPIO15 / RXD0 (pin fisico 10)
 *   ESP32 GPIO18 (RX) <- Pi GPIO14 / TXD0 (pin fisico 8)
 *   GND comun.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "uart_tx";

#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    17
#define UART_RX_PIN    18
#define UART_BAUD      115200
#define UART_BUF_SIZE  256

#define SOF       0x7E
#define EOF_BYTE  0x7F
#define ACK_SOF   0xA5
#define ACK_EOF   0x5A
#define MAX_PAYLOAD 64

#define ACK_TIMEOUT_MS 500
#define MAX_RETRIES    5

/* CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF */
static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

/* Construye la trama de datos en 'frame'; devuelve su longitud total. */
static size_t build_frame(uint8_t *frame, uint8_t seq,
                          const uint8_t *payload, uint8_t len)
{
    size_t idx = 0;
    frame[idx++] = SOF;
    frame[idx++] = seq;
    frame[idx++] = len;
    memcpy(&frame[idx], payload, len);
    idx += len;

    uint16_t crc = crc16(&frame[1], 2 + len);  /* protege SEQ+LEN+PAYLOAD */
    frame[idx++] = (crc >> 8) & 0xFF;
    frame[idx++] = crc & 0xFF;
    frame[idx++] = EOF_BYTE;
    return idx;
}

/* Espera un ACK valido para 'expected_seq'. Devuelve true si llega a tiempo. */
static bool wait_for_ack(uint8_t expected_seq, int timeout_ms)
{
    uint8_t b;
    int waited = 0;
    const int step = 10;
    enum { A_SOF, A_SEQ, A_EOF } st = A_SOF;
    uint8_t rx_seq = 0;

    while (waited < timeout_ms) {
        int n = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(step));
        if (n == 1) {
            switch (st) {
                case A_SOF: if (b == ACK_SOF) st = A_SEQ; break;
                case A_SEQ: rx_seq = b; st = A_EOF; break;
                case A_EOF:
                    if (b == ACK_EOF && rx_seq == expected_seq) return true;
                    st = A_SOF;
                    break;
            }
        } else {
            waited += step;
        }
    }
    return false;
}

static void uart_setup(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void app_main(void)
{
    uart_setup();
    ESP_LOGI(TAG, "Emisor con ACK + retransmision listo.");

    uint8_t seq = 0;
    uint32_t counter = 0;

    while (1) {
        uint8_t payload[MAX_PAYLOAD];
        int plen = snprintf((char *)payload, sizeof(payload),
                            "lectura %lu", (unsigned long)counter);

        uint8_t frame[8 + MAX_PAYLOAD];
        size_t flen = build_frame(frame, seq, payload, (uint8_t)plen);

        bool acked = false;
        for (int attempt = 0; attempt <= MAX_RETRIES && !acked; attempt++) {
            uart_write_bytes(UART_PORT, frame, flen);
            if (attempt == 0)
                ESP_LOGI(TAG, "TX seq=%u '%s'", seq, payload);
            else
                ESP_LOGW(TAG, "Reintento %d seq=%u", attempt, seq);

            acked = wait_for_ack(seq, ACK_TIMEOUT_MS);
        }

        if (acked)
            ESP_LOGI(TAG, "ACK seq=%u OK", seq);
        else
            ESP_LOGE(TAG, "seq=%u SIN ACK tras %d reintentos", seq, MAX_RETRIES);

        seq = (seq + 1) & 0xFF;
        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
