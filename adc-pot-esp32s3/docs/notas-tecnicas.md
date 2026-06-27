# Notas técnicas

Observaciones extraídas del comportamiento **real** del hardware durante las
pruebas, no de la teoría. Cada una tiene implicaciones para las siguientes fases
del proyecto.

## 1. Cuantización (resolución finita)

El ADC trabaja a 12 bits, así que parte el rango de entrada en 2¹² = 4096 niveles
discretos. Con un rango de ~0–3.3 V, cada nivel (cada "cuenta") vale:

```
3300 mV / 4095 ≈ 0.806 mV por cuenta
```

No es posible distinguir variaciones de tensión más finas que ese paso. Esto es
la cuantización, y es una limitación intrínseca de cualquier sistema de
adquisición digital.

## 2. Ruido del ADC

Con el potenciómetro **inmóvil**, las lecturas no son constantes:

```
raw=1939, 1945, 1942, 1941, 1943, 1939, ...
```

Hay una oscilación de ±3 cuentas alrededor del valor medio. Es ruido real:
eléctrico, térmico y del propio convertidor. Ningún ADC entrega un valor
perfectamente estable.

**Implicación de diseño:** las lecturas nunca deben compararse por igualdad
exacta (`==`). En la Fase 2 será necesario:

- **Filtrar** (p. ej. promediar N muestras) para reducir el ruido.
- Usar **umbrales con histéresis** en lugar de un único punto de corte, para
  evitar oscilaciones rápidas (chattering) cuando la señal está cerca del umbral.

## 3. Saturación (clipping) en los extremos

En el tope del recorrido, la lectura se queda "pegada" en el máximo durante
varias muestras consecutivas:

```
raw=4095, 4095, 4095, 4095, 4095, ...
```

El ADC no puede representar nada por encima de su valor máximo. Si la tensión real
excede el rango, la medida se satura.

**Implicación de seguridad funcional:** un valor pegado al máximo (o al mínimo)
es ambiguo. Puede significar:

- El sensor está legítimamente en su extremo, o
- El sensor está averiado, desconectado o en cortocircuito.

Un sistema robusto debe tratar los valores saturados como sospechosos y aplicar
lógica adicional (duración, plausibilidad, comparación con otras señales) antes de
confiar en ellos.

## 4. Lectura cruda vs. calibrada

La conversión a milivoltios usada en el código es una regla de tres ingenua:

```c
int mv_approx = (raw * 3300) / 4095;
```

Esto asume que el ADC es perfectamente lineal y que el fondo de escala es
exactamente 3300 mV. En la práctica ninguna de las dos cosas es cierta: cada chip
tiene un offset y una ganancia ligeramente distintos.

ESP-IDF ofrece una API de calibración (`esp_adc/adc_cali.h`) que usa valores de
caracterización grabados en fábrica (eFuses) para devolver milivoltios corregidos.
La diferencia entre la medida cruda y la calibrada es precisamente el tipo de
rigor que distingue una medida "de juguete" de una medida de instrumentación.

**Mejora pendiente:** añadir la ruta de calibración y comparar `mv_approx` con el
valor calibrado.
