# fase2-monitor-fsm

Monitor de nivel sobre ESP32-S3: lee un potenciómetro por ADC, filtra el ruido,
y gobierna una alarma sonora intermitente mediante una máquina de estados con
histéresis, detección de fallos y watchdog hardware.

Segundo hito de la ruta de aprendizaje. Si la Fase 1 fue "leer un sensor", esta
es "construir un sistema reactivo y tolerante a fallos".

## Qué hace

Muestrea el potenciómetro cada 100 ms, promedia las últimas 8 lecturas, y según
el valor filtrado activa o no una alarma intermitente en el buzzer:

- Por debajo del nivel bajo → silencio (`NORMAL_LOW`).
- Por encima del nivel alto → alarma intermitente (`NORMAL_HIGH`).
- Entre ambos umbrales → mantiene el estado anterior (histéresis).
- Ante fallos de lectura repetidos → salida segura (`FAULT`).

## Conceptos implementados

| Concepto | Dónde |
|----------|-------|
| Filtrado digital (media móvil O(1)) | `filter_update()` |
| Máquina de estados explícita | `system_state_t` + `switch` en el bucle |
| Histéresis (doble umbral) | `THRESH_HIGH` / `THRESH_LOW` |
| PWM para buzzer pasivo | periférico LEDC, `buzzer_*()` |
| Temporización no bloqueante | comparación contra `esp_timer_get_time()` |
| Detección de fallos | contador `consec_fails` → `ST_FAULT` |
| Watchdog hardware (TWDT) | `esp_task_wdt_*()` |

## Hardware

| Componente | Conexión |
|------------|----------|
| Potenciómetro | 3V3 / GPIO1 (ADC1_CH0) / GND |
| Buzzer pasivo | GPIO4 / GND |

```
   Potenciómetro            ESP32-S3            Buzzer pasivo
   izq  ───── 3V3            GPIO1 ──── centro
   der  ───── GND            GPIO4 ──── +
                             GND   ──── -
```

> El buzzer es **pasivo**: no tiene oscilador propio, así que se excita con una
> onda cuadrada de 2 kHz generada por PWM (LEDC). Un buzzer activo sonaría con
> tensión continua; este no.

## Compilar y flashear

```bash
get_idf
idf.py set-target esp32s3
idf.py -p /dev/ttyACM0 flash monitor
```

## Documentación

- [`docs/diseno.md`](docs/diseno.md) — máquina de estados, histéresis y
  decisiones de diseño, con análisis de los datos medidos.
- [`docs/requisitos.md`](docs/requisitos.md) — especificación de requisitos con
  trazabilidad requisito → código.

## Construcción incremental

El sistema se construyó en tres incrementos verificables:

- **A** — Filtrado por media móvil (reducción de ruido medida).
- **B** — Máquina de estados + histéresis + alarma intermitente.
- **C** — Detección de fallos (`FAULT`) + watchdog hardware.
