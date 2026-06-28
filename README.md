# Embedded Systems

Portafolio de proyectos personales de sistemas embebidos, centrado en adquisición
de datos, programación en C de bajo nivel y prácticas de seguridad funcional.

Los proyectos siguen una ruta de aprendizaje progresiva: cada uno construye sobre
el anterior hasta formar un sistema de adquisición y monitorización completo, de
extremo a extremo (sensor → microcontrolador → comunicación fiable → host Linux).

## Hardware de trabajo

- **ESP32-S3** — microcontrolador principal (ESP-IDF, C sobre FreeRTOS)
- **Raspberry Pi 3B+** — host Linux (módulos de kernel, recepción de datos)
- Componentes discretos: potenciómetro, fotorresistor, 74HC595, transistor NPN,
  pulsador, buzzer pasivo
- Instrumentación: multímetro, fuente de alimentación

## Ruta de proyectos

| Fase | Proyecto | Conceptos | Estado |
|------|----------|-----------|--------|
| 1 | [adc-pot-esp32s3](adc-pot-esp32s3/) | ADC, cuantización, ruido, saturación, requisitos trazables | ✅ Completado |
| 2 | [fase2-monitor-fsm](fase2-monitor-fsm/) | FSM, filtrado, histéresis, watchdog, detección de fallos | ✅ Completado |
| 3 | [fase3-uart-protocolo](fase3-uart-protocolo/) | UART, framing, CRC, ACK/retransmisión, números de secuencia | ✅ Completado |
| 4 | *(planificado)* Módulo de kernel receptor | Linux char device driver, `/dev`, I2C/SPI | ⬜ |

## Enfoque

Cada proyecto incluye, además del código:

- **Documentación técnica** basada en el comportamiento real del hardware medido,
  no solo en la teoría.
- **Especificación de requisitos** con identificadores y trazabilidad
  requisito → código, siguiendo prácticas de desarrollo orientado a la seguridad
  funcional.

## Licencia

Código bajo licencia MIT. Ver [LICENSE](LICENSE).
