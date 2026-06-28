/*
 * Fase 3 - Receptor (Raspberry Pi): protocolo UART fiable.
 *
 * Parsea tramas con SEQ + CRC16 mediante una maquina de estados, envia ACK
 * por cada trama valida, y detecta duplicados por numero de secuencia.
 * Incluye inyeccion de fallos para demostrar la recuperacion: descarta 1
 * de cada N ACKs, forzando retransmisiones en el emisor.
 *
 * Uso:  ./rx [N]
 *   ./rx      -> modo normal (nunca descarta ACKs)
 *   ./rx 4    -> descarta 1 de cada 4 ACKs
 *
 * Compilar:  gcc -Wall -Wextra -o rx rx.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define SERIAL_PORT "/dev/serial0"
#define BAUD        B115200

#define SOF       0x7E
#define EOF_BYTE  0x7F
#define ACK_SOF   0xA5
#define ACK_EOF   0x5A
#define MAX_PAYLOAD 64

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

static void send_ack(int fd, uint8_t seq)
{
    uint8_t ack[3] = { ACK_SOF, seq, ACK_EOF };
    write(fd, ack, sizeof(ack));
}

typedef enum {
    WAIT_SOF, READ_SEQ, READ_LEN, READ_PAYLOAD, READ_CRC_HI, READ_CRC_LO, READ_EOF,
} parser_state_t;

int main(int argc, char *argv[])
{
    int drop_every = 0;
    if (argc >= 2) {
        drop_every = atoi(argv[1]);
        if (drop_every < 0) drop_every = 0;
    }
    if (drop_every > 0)
        printf("Modo inyeccion de fallos: descarto 1 de cada %d ACKs.\n", drop_every);
    else
        printf("Modo normal: nunca descarto ACKs.\n");

    int fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        fprintf(stderr, "Error abriendo %s: %s\n", SERIAL_PORT, strerror(errno));
        return EXIT_FAILURE;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "tcgetattr: %s\n", strerror(errno));
        close(fd); return EXIT_FAILURE;
    }
    cfsetispeed(&tty, BAUD);
    cfsetospeed(&tty, BAUD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(INLCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
        close(fd); return EXIT_FAILURE;
    }

    printf("Receptor escuchando en %s...\n", SERIAL_PORT);

    parser_state_t state = WAIT_SOF;
    uint8_t  payload[MAX_PAYLOAD];
    uint8_t  seq = 0, len = 0, payload_idx = 0;
    uint16_t rx_crc = 0;

    int last_seq = -1;
    unsigned long ok_count = 0, dup_count = 0, err_count = 0;
    unsigned long ack_dropped = 0, frames_valid = 0;

    uint8_t byte;
    while (1) {
        int n = read(fd, &byte, 1);
        if (n <= 0) { if (n < 0) fprintf(stderr, "read: %s\n", strerror(errno)); continue; }

        switch (state) {
            case WAIT_SOF:
                if (byte == SOF) state = READ_SEQ;
                break;
            case READ_SEQ:
                seq = byte; state = READ_LEN;
                break;
            case READ_LEN:
                len = byte;
                if (len > MAX_PAYLOAD) { state = WAIT_SOF; }
                else { payload_idx = 0; state = (len == 0) ? READ_CRC_HI : READ_PAYLOAD; }
                break;
            case READ_PAYLOAD:
                payload[payload_idx++] = byte;
                if (payload_idx >= len) state = READ_CRC_HI;
                break;
            case READ_CRC_HI:
                rx_crc = (uint16_t)byte << 8; state = READ_CRC_LO;
                break;
            case READ_CRC_LO:
                rx_crc |= byte; state = READ_EOF;
                break;
            case READ_EOF:
                if (byte == EOF_BYTE) {
                    uint8_t check[2 + MAX_PAYLOAD];
                    check[0] = seq;
                    check[1] = len;
                    memcpy(&check[2], payload, len);
                    uint16_t calc = crc16(check, 2 + len);

                    if (calc == rx_crc) {
                        frames_valid++;
                        int drop = (drop_every > 0) && (frames_valid % drop_every == 0);

                        if ((int)seq == last_seq) {
                            dup_count++;
                            if (drop) {
                                ack_dropped++;
                                printf("[DUP] seq=%u -> ACK DESCARTADO (dropped=%lu)\n",
                                       seq, ack_dropped);
                            } else {
                                printf("[DUP] seq=%u -> reenvio ACK\n", seq);
                                send_ack(fd, seq);
                            }
                        } else {
                            payload[len] = '\0';
                            ok_count++;
                            if (drop) {
                                ack_dropped++;
                                printf("[OK]  seq=%u '%s' -> ACK DESCARTADO (ok=%lu dropped=%lu)\n",
                                       seq, payload, ok_count, ack_dropped);
                            } else {
                                printf("[OK]  seq=%u '%s' (ok=%lu dup=%lu)\n",
                                       seq, payload, ok_count, dup_count);
                                send_ack(fd, seq);
                            }
                            last_seq = seq;
                        }
                    } else {
                        err_count++;
                        printf("[ERR] CRC seq=%u rx=0x%04X calc=0x%04X (err=%lu)\n",
                               seq, rx_crc, calc, err_count);
                    }
                } else {
                    err_count++;
                    printf("[ERR] EOF invalido 0x%02X (err=%lu)\n", byte, err_count);
                }
                state = WAIT_SOF;
                break;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
