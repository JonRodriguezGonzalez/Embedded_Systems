# Diseño del protocolo

Documento de diseño del protocolo de comunicación fiable. Recoge las decisiones
y los compromisos, ilustrados con el comportamiento real medido entre las dos
placas.

## Capas del protocolo

El protocolo se construye en capas, cada una resolviendo un problema que la
anterior deja abierto.

### 1. Framing

Por el cable viaja un flujo continuo de bytes sin fronteras. El framing delimita
cada mensaje con un byte de inicio (`SOF = 0x7E`) y uno de fin (`EOF = 0x7F`),
más un byte de longitud (`LEN`). El receptor implementa una máquina de estados de
siete estados (`WAIT_SOF → READ_SEQ → READ_LEN → READ_PAYLOAD → READ_CRC_HI →
READ_CRC_LO → READ_EOF`) que se sincroniza con el flujo: si arranca a mitad de una
trama, descarta bytes hasta encontrar un `SOF` y comienza limpio.

Esto se verificó en la práctica: al arrancar el receptor a mitad de transmisión,
la primera lectura produjo un único `[ERR] EOF invalido` y, a partir de ahí, el
parser se sincronizó sin más errores.

### 2. Integridad: CRC16

El ruido eléctrico puede voltear bits en tránsito. El CRC-16/CCITT-FALSE
(polinomio `0x1021`, valor inicial `0xFFFF`) trata el mensaje como un número
binario y calcula el resto de una división polinómica con XOR. El emisor lo
incluye en la trama; el receptor lo recalcula sobre `SEQ + LEN + PAYLOAD` y
compara. Un CRC16 detecta todos los errores de 1 y 2 bits, todos los de número
impar de bits, y todas las ráfagas de hasta 16 bits.

El CRC protege también el número de secuencia: si el `SEQ` de un reenvío se
corrompiera, el receptor lo detectaría en vez de confundirse de trama.

### 3. Confirmación: ACK

El receptor responde a cada trama válida con un ACK de 3 bytes que incluye el
número de secuencia confirmado. Si el CRC falla, el receptor **no** envía ACK:
el silencio es, en sí mismo, una señal ("no me llegó bien, reenvía").

### 4. Recuperación: timeout + retransmisión

El emisor, tras enviar, espera el ACK hasta `ACK_TIMEOUT_MS` (500 ms). Si no
llega, reenvía la misma trama, hasta `MAX_RETRIES` (5) veces. Esto recupera tanto
tramas perdidas como ACKs perdidos.

### 5. Duplicados: número de secuencia

Si un ACK se pierde, el emisor reenvía una trama que el receptor **ya procesó**.
El receptor guarda la última secuencia procesada (`last_seq`); si llega una
repetida, la reconoce como duplicado: no procesa el dato otra vez, pero reenvía
el ACK para que el emisor avance. Sin esto, un ACK perdido provocaría datos
duplicados.

## Compromiso de diseño: fiabilidad vs. latencia

La fiabilidad cuesta tiempo. Sin ACK, cada trama tardaba un periodo fijo. Con
ACK + retransmisión, una trama cuyo ACK se pierde tarda hasta
`(reintentos × timeout)` adicionales. Cuando todos los ACKs se perdían (receptor
apagado), cada trama consumía sus 5 reintentos × 500 ms antes de rendirse, ~2.5 s
extra por trama.

Los parámetros `ACK_TIMEOUT_MS` y `MAX_RETRIES` ajustan este equilibrio:
- Timeout más corto → reacción más rápida a pérdidas, pero riesgo de reenviar
  tramas cuyo ACK simplemente venía con retraso (falsos timeouts).
- Más reintentos → más tolerancia a fallos persistentes, pero más latencia en el
  peor caso.

Para esta aplicación (telemetría a 1 trama/s) los valores son holgados. Un sistema
de tiempo real más exigente requeriría afinarlos según la latencia del canal.

## Validación con inyección de fallos

El receptor puede descartar 1 de cada N ACKs (argumento de línea de comandos),
simulando pérdidas en el canal de retorno. Con `./rx 4` se observó el ciclo
completo, correlacionado entre ambas máquinas:

```
Pi:     [OK]  seq=16 ... -> ACK DESCARTADO
ESP32:  Reintento 1 seq=16
ESP32:  ACK seq=16 OK
Pi:     [DUP] seq=16 -> reenvio ACK
```

Resultado clave: el contador de lecturas únicas (`ok`) avanzó exactamente una vez
por dato, y el de duplicados (`dup`) absorbió cada reenvío. Ningún dato se perdió
ni se procesó dos veces, incluso descartando un 25% de los ACKs.

## Detección sin corrección

El CRC detecta corrupción pero no la corrige: sabe que *algo* cambió, no *qué*.
Por eso se combina con retransmisión: detección (CRC) + recuperación
(reenvío) = entrega fiable. Las dos piezas son complementarias, no alternativas.
