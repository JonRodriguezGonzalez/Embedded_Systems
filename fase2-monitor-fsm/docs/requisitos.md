# Especificación de requisitos

Especificación del monitor de nivel, con identificadores y trazabilidad
requisito → código, siguiendo la práctica de desarrollo orientado a la seguridad
funcional.

## Convenciones

- **DEBE** (shall): obligatorio. **DEBERÍA** (should): recomendado.
- Identificadores: `REQ-<área>-<nº>`.

## Funcionales

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-FUN-01 | El sistema DEBE muestrear el ADC con un periodo nominal de 100 ms. | Hecho |
| REQ-FUN-02 | El sistema DEBE filtrar la señal mediante media móvil de 8 muestras. | Hecho |
| REQ-FUN-03 | El sistema DEBE activar una alarma intermitente cuando el valor filtrado supere el umbral alto. | Hecho |
| REQ-FUN-04 | El sistema DEBE silenciar la alarma cuando el valor filtrado caiga por debajo del umbral bajo. | Hecho |
| REQ-FUN-05 | La alarma DEBE ser intermitente con semiperiodo de 200 ms. | Hecho |

## Robustez / seguridad funcional

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-ROB-01 | El sistema DEBE usar histéresis (dos umbrales) para evitar conmutación espuria. | Hecho |
| REQ-ROB-02 | El sistema DEBE comprobar el código de retorno de cada lectura del ADC. | Hecho |
| REQ-ROB-03 | El sistema DEBE entrar en estado FAULT tras 5 lecturas fallidas consecutivas. | Hecho |
| REQ-ROB-04 | En FAULT, la salida DEBE ser segura (alarma apagada, estado conocido). | Hecho |
| REQ-ROB-05 | El sistema DEBE recuperarse de FAULT al recibir lecturas válidas. | Hecho |
| REQ-ROB-06 | El bucle principal NO DEBE bloquearse en esperas largas. | Hecho |
| REQ-ROB-07 | Un valor saturado (0 o máximo) NO DEBE tratarse como fallo por sí solo. | Hecho |

## Watchdog

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-WDT-01 | El sistema DEBE armar un watchdog hardware con tiempo límite de 3 s. | Hecho |
| REQ-WDT-02 | El bucle principal DEBE alimentar el watchdog en cada iteración. | Hecho |
| REQ-WDT-03 | Si el bucle se cuelga, el watchdog DEBE reiniciar el sistema. | Hecho |

## Trazabilidad requisito → código

| Requisito  | Ubicación |
|------------|-----------|
| REQ-FUN-01 | `vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS))` |
| REQ-FUN-02 | `filter_update()` |
| REQ-FUN-03 | transición `ST_NORMAL_LOW → ST_NORMAL_HIGH` |
| REQ-FUN-04 | transición `ST_NORMAL_HIGH → ST_NORMAL_LOW` |
| REQ-FUN-05 | bloque `BEEP_PERIOD_MS` con `buzzer_set()` |
| REQ-ROB-01 | `THRESH_HIGH` / `THRESH_LOW` |
| REQ-ROB-02 | `if (err == ESP_OK)` |
| REQ-ROB-03 | contador `consec_fails` → `ST_FAULT` |
| REQ-ROB-04 | rama de entrada a FAULT (`buzzer_set(false)`) |
| REQ-ROB-05 | recuperación `if (state == ST_FAULT)` con `filter_reset()` |
| REQ-ROB-06 | temporización con `now_ms()` en lugar de `delay()` |
| REQ-ROB-07 | ausencia deliberada de lógica que trate la saturación como fallo |
| REQ-WDT-01 | `esp_task_wdt_init()` / `esp_task_wdt_config_t` |
| REQ-WDT-02 | `esp_task_wdt_reset()` al inicio del bucle |
| REQ-WDT-03 | `.trigger_panic = true` |

## Pendiente para fases siguientes

En la Fase 3 los eventos de transición y los datos de medida se transmitirán a un
host por un protocolo de comunicación fiable (framing, CRC, ACK).
