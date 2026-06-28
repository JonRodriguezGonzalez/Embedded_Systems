# Especificación de requisitos

Requisitos del protocolo de comunicación fiable, con trazabilidad a código en los
dos lados (E = emisor ESP32, R = receptor Pi).

## Funcionales

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-FUN-01 | El emisor DEBE delimitar cada mensaje con bytes SOF y EOF. | Hecho |
| REQ-FUN-02 | Cada trama DEBE incluir un número de secuencia. | Hecho |
| REQ-FUN-03 | Cada trama DEBE incluir un CRC16 sobre SEQ+LEN+PAYLOAD. | Hecho |
| REQ-FUN-04 | El receptor DEBE confirmar cada trama válida con un ACK. | Hecho |

## Integridad y robustez

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-ROB-01 | El receptor DEBE recalcular el CRC y descartar tramas corruptas. | Hecho |
| REQ-ROB-02 | Ante CRC inválido, el receptor NO DEBE enviar ACK. | Hecho |
| REQ-ROB-03 | El parser DEBE sincronizarse con el flujo si arranca a mitad de trama. | Hecho |
| REQ-ROB-04 | El parser DEBE rechazar longitudes mayores que el buffer. | Hecho |
| REQ-ROB-05 | El emisor DEBE retransmitir si no recibe ACK antes del timeout. | Hecho |
| REQ-ROB-06 | El emisor DEBE limitar el número de reintentos. | Hecho |
| REQ-ROB-07 | El receptor DEBE detectar duplicados y no procesarlos dos veces. | Hecho |
| REQ-ROB-08 | Ante un duplicado, el receptor DEBE reenviar el ACK. | Hecho |

## Verificación / pruebas

| ID         | Requisito | Estado |
|------------|-----------|--------|
| REQ-TST-01 | El receptor DEBE poder inyectar fallos (descartar ACKs) para pruebas. | Hecho |
| REQ-TST-02 | El parámetro de inyección DEBE configurarse por línea de comandos. | Hecho |

## Trazabilidad requisito → código

| Requisito  | Lado | Ubicación |
|------------|------|-----------|
| REQ-FUN-01 | E    | `build_frame()` (SOF, EOF) |
| REQ-FUN-02 | E    | `seq` en `build_frame()` |
| REQ-FUN-03 | E    | `crc16(&frame[1], 2+len)` |
| REQ-FUN-04 | R    | `send_ack()` |
| REQ-ROB-01 | R    | comparación `calc == rx_crc` |
| REQ-ROB-02 | R    | rama `else` sin `send_ack` |
| REQ-ROB-03 | R    | estado `WAIT_SOF` |
| REQ-ROB-04 | R    | `if (len > MAX_PAYLOAD)` |
| REQ-ROB-05 | E    | `wait_for_ack()` + bucle de reintentos |
| REQ-ROB-06 | E    | `attempt <= MAX_RETRIES` |
| REQ-ROB-07 | R    | `if ((int)seq == last_seq)` |
| REQ-ROB-08 | R    | `send_ack()` en la rama de duplicado |
| REQ-TST-01 | R    | variable `drop` + `frames_valid % drop_every` |
| REQ-TST-02 | R    | `atoi(argv[1])` |

## Pendiente para fases siguientes

En la Fase 4, los datos recibidos y verificados en la Pi se expondrán a través de
un módulo de kernel de Linux como un dispositivo de caracteres en `/dev`.
