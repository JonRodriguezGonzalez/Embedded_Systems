# Diseño y decisiones técnicas

Documento de diseño del monitor de nivel. Las cifras y observaciones provienen
del comportamiento **real** del sistema medido por el puerto serie, no de la
teoría.

## Máquina de estados

```
                  ┌──────┐
                  │ INIT │
                  └──┬───┘
                     │ arranque
                     v
        ┌────────────────────────┐
        │      NORMAL_LOW         │  buzzer apagado
        │   (actuador en reposo)  │
        └───────────┬────────────┘
              media > THRESH_HIGH │ ▲ media < THRESH_LOW
                     v            │
        ┌────────────────────────┐
        │      NORMAL_HIGH        │  alarma intermitente
        │   (alarma sonora)       │
        └────────────────────────┘

   Desde cualquier estado normal:
   N fallos de lectura consecutivos ──> FAULT (salida segura)
   lectura válida ──> recuperación a NORMAL_LOW
```

Los estados son un `enum` explícito y las transiciones viven en un `switch`
dentro del bucle principal. No se usan banderas booleanas sueltas: un estado
explícito es trazable y testeable, que es lo que se busca en sistemas con
requisitos de seguridad.

## Filtrado: media móvil

### Por qué

El ADC tiene ruido intrínseco. Con el potenciómetro inmóvil, la lectura cruda
oscila varias cuentas. Actuar directamente sobre la lectura cruda haría que el
sistema reaccionara a ese ruido.

### Resultado medido

Con el potenciómetro quieto, la lectura cruda oscilaba aproximadamente en un
rango de ±5 cuentas (p. ej. 1834–1848), mientras que la salida filtrada se
mantenía estable dentro de ±1–2 cuentas (1844–1846). El filtro elimina la mayor
parte del ruido.

### Coste: latencia

El filtrado introduce retardo. En un cambio brusco de la entrada (giro rápido
del potenciómetro), la salida cruda salta de inmediato pero la filtrada tarda
del orden de N muestras en alcanzarla. Con N = 8 y muestreo a 100 ms, eso son
unos ~700 ms de retardo ante un escalón. Es el compromiso fundamental de
cualquier filtro: **más suavidad implica más latencia**. La elección de N = 8 es
un punto intermedio razonable para esta aplicación.

### Implementación O(1)

En lugar de recorrer el buffer en cada muestra (O(N)), se mantiene una suma
acumulada: al insertar una muestra se resta la que sale y se suma la que entra.
Coste constante por muestra, independientemente del tamaño de la ventana.

## Histéresis

### El problema del umbral único

Con un único umbral, cuando la señal ronda ese valor el ruido la cruza arriba y
abajo repetidamente, y el actuador conmuta sin control (*chattering*).

### La solución: dos umbrales

- Se sube a `NORMAL_HIGH` cuando la media supera `THRESH_HIGH` (2800).
- Se baja a `NORMAL_LOW` solo cuando la media cae por debajo de `THRESH_LOW` (1200).
- En la banda intermedia (1200–2800) el sistema **mantiene** su estado.

### Validación medida

En una prueba se mantuvo la señal filtrada rondando 2700–2800 (justo dentro de
la banda muerta, incluso tocando 2799) durante más de 30 segundos. El sistema
permaneció en `NORMAL_LOW` sin una sola conmutación espuria. Un diseño con
umbral único habría producido conmutación continua en esa situación.

## Alarma intermitente sin bloqueo

El buzzer pasivo se excita con PWM (LEDC) a 2 kHz: esa frecuencia es el tono. El
patrón intermitente (pitido/silencio cada 200 ms) NO se hace con `delay()`, que
congelaría el muestreo. En su lugar se compara el tiempo actual
(`esp_timer_get_time()`) contra el instante del último cambio. Así el bucle sigue
muestreando y reaccionando mientras la alarma suena. Regla general: **el bucle
principal de un sistema reactivo nunca debe bloquearse en esperas largas**.

## Detección de fallos

### Qué cuenta (y qué no) como fallo

Un valor saturado (`raw = 0` o `raw = 4095`) **no** es un fallo: corresponde a
los extremos legítimos del potenciómetro, y se observa con normalidad durante el
uso. Tratarlo como fallo daría falsos positivos.

Lo que sí se considera fallo es un **error de lectura del ADC** (código de
retorno distinto de `ESP_OK`). Para evitar reaccionar a un fallo transitorio
aislado, se exige que se acumulen `MAX_CONSEC_FAILS` (5) lecturas fallidas
consecutivas antes de declarar `FAULT`.

### Salida segura y recuperación

Al entrar en `FAULT`, la salida segura es: buzzer apagado y estado conocido. La
recuperación es controlada: en cuanto vuelve a llegar una lectura válida, se
reinicia el filtro (para no arrastrar datos previos al fallo) y se vuelve a
`NORMAL_LOW`.

## Watchdog hardware (TWDT)

El Task Watchdog Timer vigila la tarea del bucle principal. Esta debe
"alimentarlo" (`esp_task_wdt_reset()`) en cada iteración. Si el bucle se cuelga y
deja de alimentarlo durante más de `WDT_TIMEOUT_S` (3 s), el watchdog dispara un
panic y reinicia el chip.

Es el mecanismo de último recurso: protege precisamente contra los cuelgues que
el propio software no puede detectar, porque está colgado. La detección de fallos
(arriba) maneja errores que el código *sí* puede observar; el watchdog cubre los
que no.

### Prueba sugerida

Insertar temporalmente un bucle infinito (`while(1){}`) dentro del bucle
principal deja de alimentar el watchdog; a los ~3 s el log muestra
`Task watchdog got triggered` seguido de un reinicio. Demuestra la
autorrecuperación.
