# adc-pot-esp32s3

Lectura analógica de un potenciómetro mediante el ADC de una ESP32-S3, usando
ESP-IDF en C. Primer hito de una ruta de aprendizaje de sistemas embebidos
orientada a adquisición de datos y seguridad funcional.

Este proyecto es deliberadamente pequeño: el objetivo no es la complejidad, sino
hacer **bien** lo fundamental — entender el periférico, validar las lecturas y
documentar el comportamiento real del hardware (ruido, saturación, cuantización).

## Qué hace

Lee periódicamente la tensión del cursor de un potenciómetro conectado a un canal
del ADC1, e imprime por el puerto serie el valor crudo (0–4095) y su conversión
aproximada a milivoltios.

```
I (266)  adc_pot: ADC inicializado. Gira el potenciometro.
I (266)  adc_pot: raw=2009  ~1618 mV
I (9266) adc_pot: raw= 367  ~ 295 mV
I (16266) adc_pot: raw=4095  ~3300 mV
```

## Hardware

| Componente        | Detalle                                  |
|-------------------|------------------------------------------|
| MCU               | ESP32-S3 (USB-Serial-JTAG nativo)        |
| Sensor            | Potenciómetro (~10 kΩ) como divisor de tensión |
| Alimentación      | 3V3 de la placa                          |

### Conexionado

```
   Potenciómetro                ESP32-S3
   ┌───────────┐
   │  izq  ────────────────────  3V3
   │  centro ──────────────────  GPIO1  (ADC1_CH0)
   │  der  ────────────────────  GND
   └───────────┘
```

El potenciómetro forma un divisor de tensión: el cursor (pata central) entrega
una fracción de 3V3 entre 0 V y 3V3 según la posición. Ese voltaje es lo que
convierte el ADC.

> Aviso: la pata de alimentación va a **3V3**, nunca a 5V. Los pines ADC de la
> ESP32-S3 no son tolerantes a 5 V.

## Compilar y flashear

```bash
get_idf                       # activar entorno ESP-IDF
idf.py set-target esp32s3
idf.py -p /dev/ttyACM0 flash monitor
```

(Salir del monitor: `Ctrl + ]`)

## Configuración del ADC

| Parámetro   | Valor              | Significado                          |
|-------------|--------------------|--------------------------------------|
| Unidad      | ADC1               | ADC2 entra en conflicto con el WiFi  |
| Canal       | CH0 (GPIO1)        | Pin de entrada                       |
| Resolución  | 12 bits (0–4095)   | 4096 niveles de cuantización         |
| Atenuación  | 12 dB              | Rango de entrada ~0–3.3 V            |

## Notas técnicas

Ver [`docs/notas-tecnicas.md`](docs/notas-tecnicas.md) para el análisis del ruido
del ADC, la saturación en los extremos y la diferencia entre lectura cruda y
calibrada.

Ver [`docs/requisitos.md`](docs/requisitos.md) para la especificación de
requisitos del módulo.

## Ruta de aprendizaje

Este es el hito 1 de una ruta más amplia hacia un sistema de adquisición y
monitorización embebido completo:

- [x] **Fase 1** — Lectura ADC de un sensor analógico *(este repo)*
- [ ] Fase 2 — Muestreo con máquina de estados + filtrado + watchdog
- [ ] Fase 3 — Protocolo de comunicación fiable (UART con framing y CRC)
- [ ] Fase 4 — Módulo de kernel Linux receptor en Raspberry Pi
