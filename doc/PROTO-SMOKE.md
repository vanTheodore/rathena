# Prueba de humo por protocolo, sin cliente (`tools/proto-smoke.ps1`)

Recorre login → auth de char → creación → selección → entrada al
map-server → salida limpia, solo con sockets. No necesita RagexeRE, GRFs,
WARP, Docker ni WSL.

## Alcance (5 pruebas)

`1-login-accept`, `2a-auth-echo`, `2b-char-list`, `4-map-notify`,
`5-map-data`. Cubre crear, seleccionar, entrar al mapa y el guardado que la
salida limpia dispara en el servidor. Nada más.

## Preparación (tuya, el script no inicia servicios)

- Compila con `-j2` como siempre; el script no compila ni arranca nada.
- Apunta los tres servidores a una base de datos **desechable** (nunca
  producción) con el esquema de `sql-files/main.sql`.
- Crea en esa BD una cuenta de prueba con un personaje en el slot 0
  (cualquier nombre); el script la reutiliza en cada corrida.
- El map-server necesita un `map_cache.dat` válido (`db/map_cache.dat`
  ya sirve para el mapa inicial).

## Ejecución

```powershell
powershell -ExecutionPolicy Bypass -File tools/proto-smoke.ps1 -User <test-user> -Pass <test-pass> -Char <fixture-char> -NewChar <fresh-char> [-Slot 1]
```

`-User/-Pass/-Char/-NewChar` son obligatorios y no tienen valores por defecto: así
ningún secreto ni dato personal vive en el repositorio. Solo `-LoginPort`
(6900), `-LoginHost` (127.0.0.1) y `-ClientVer` (debe coincidir con el
`PACKETVER` del build) tienen defecto. Los puertos de char y map se
descubren por protocolo. Éxito = `FAILURES=0` y salida 0; cualquier fallo
relevante sale distinto de cero.

## Limpieza

Cerrar las conexiones ya dispara el guardado en el servidor. Para dejar la
BD desechable como estaba, elimina el personaje/cuenta de prueba o parte de
una copia fresca. Apaga los servidores y la BD al terminar.

## Seguridad

El script no ejecuta ninguna operación destructiva: abre sockets, crea como
máximo un personaje de prueba y sale. No accede a la BD directamente, no
lee archivos de credenciales y no borra nada. Ninguna prueba se salta por
llamarse `test`: lo desechable lo garantiza tu BD, no un nombre.

## Pendiente (no bloquea): borrado 0x1fb

El driver original incluía un paso de borrado que falla sin corromper nada
(la fila sobrevive intacta). Causa del lado del driver: a este `PACKETVER`
`PACKET_CH_DELETE_CHAR` lleva clave de 40 bytes (`src/common/packets.hpp`)
pero el driver enviaba 50, así que el servidor nunca procesaba el borrado;
además el bloque re-autenticaba dos veces seguidas. Quedó fuera del script
a propósito. Reactivarlo exige clave de 40 + una sola autenticación, y aun
así solo contra BD desechable con autorización explícita.
