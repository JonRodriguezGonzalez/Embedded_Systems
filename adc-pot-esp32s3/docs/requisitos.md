# Especificación de requisitos

Especificación ligera del módulo de lectura ADC, redactada al estilo de los
documentos de requisitos usados en desarrollo de sistemas con requisitos de
seguridad. El objetivo es practicar la **trazabilidad**: cada requisito tiene un
identificador y puede ligarse a un punto del código y a una verificación.

## Convenciones

- **DEBE** (shall): requisito obligatorio.
- **DEBERÍA** (should): recomendación.
- Identificadores: `REQ-<área>-<nº>`.

## Requisitos funcionales

| ID            | Requisito                                                                                  | Estado |
|---------------|--------------------------------------------------------------------------------------------|--------|
| REQ-FUN-01    | El sistema DEBE inicializar el ADC1 en el canal CH0 (GPIO1) a 12 bits y atenuación 12 dB.  | Hecho  |
| REQ-FUN-02    | El sistema DEBE leer el ADC periódicamente con un periodo nominal de 500 ms.               | Hecho  |
| REQ-FUN-03    | El sistema DEBE emitir por el log el valor crudo (0–4095) en cada lectura.                 | Hecho  |
| REQ-FUN-04    | El sistema DEBE emitir una estimación de la tensión en mV en cada lectura.                 | Hecho  |

## Requisitos de robustez / manejo de errores

| ID            | Requisito                                                                                  | Estado     |
|---------------|--------------------------------------------------------------------------------------------|------------|
| REQ-ROB-01    | El sistema DEBE comprobar el código de retorno de cada lectura del ADC.                    | Hecho      |
| REQ-ROB-02    | Ante una lectura fallida, el sistema DEBE emitir una advertencia y continuar operando.     | Hecho      |
| REQ-ROB-03    | El sistema DEBERÍA detectar y señalar valores saturados (raw = 0 o raw = 4095).            | Pendiente  |
| REQ-ROB-04    | El sistema DEBERÍA filtrar el ruido del ADC promediando múltiples muestras.                | Pendiente  |

## Requisitos de seguridad eléctrica (hardware)

| ID            | Requisito                                                                                  | Estado |
|---------------|--------------------------------------------------------------------------------------------|--------|
| REQ-HW-01     | La entrada analógica DEBE alimentarse a 3V3; NO DEBE conectarse a 5 V.                      | Hecho  |
| REQ-HW-02     | DEBE usarse el ADC1; NO DEBE usarse el ADC2 (conflicto con el subsistema de radio).         | Hecho  |

## Trazabilidad requisito → código

| Requisito   | Ubicación en el código                                  |
|-------------|--------------------------------------------------------|
| REQ-FUN-01  | `adc_oneshot_new_unit()` / `adc_oneshot_config_channel()` |
| REQ-FUN-02  | `vTaskDelay(pdMS_TO_TICKS(500))`                       |
| REQ-FUN-03  | `ESP_LOGI(... raw ...)`                                |
| REQ-FUN-04  | cálculo `mv_approx`                                    |
| REQ-ROB-01  | comprobación `if (err == ESP_OK)`                      |
| REQ-ROB-02  | rama `else` con `ESP_LOGW`                             |

## Pendientes para fases siguientes

Los requisitos `REQ-ROB-03` y `REQ-ROB-04` se implementarán en la Fase 2, cuando
se introduzca la máquina de estados con filtrado e histéresis.
