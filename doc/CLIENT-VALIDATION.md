# Validación con cliente real: requisitos y estado

Punto de partida para retomar cuando el cliente esté configurado.
No descargar, instalar ni preparar nada hasta entonces.

## Cliente requerido (confirmado)

- **RagexeRE ~2021-11-03.** Nuestra compilación define `PACKETVER_RE`
  (`20211103` cae en el intervalo auto-RE 20200902–20211118;
  `PACKETVER_MAIN_NUM` queda indefinido; sin overrides en
  `src/custom/defines_pre.hpp`). Obfuscación con keys cero.
- Parchador: **WARP (Neo-Mind/WARP)**, continuación de NEMO por su mismo
  desarrollador. No asumir lista de parches: a confirmar en ejecución.
- `sclientinfo.xml` → `127.0.0.1:6900` + carpeta `data/` primero.

## Configuración pendiente (lado servidor, gitignored)

- `conf/import/inter_conf.txt`: BD de pruebas (host/puerto/usuario/BD).
- `pincode_enabled: no`, cuenta GM (`UPDATE login SET group_id=99`).
- `map_cache.dat` REAL generado desde los GRFs del cliente con `mapcache`
  (`db/import/map_cache.dat`, ignorado por git). El stub actual (8 entradas)
  no sirve para entrar a mapas; el sintético de pruebas, tampoco (ver abajo).

## Validado con sockets (sin cliente) — 5/5 PASS

Driver: `C:\rathena-test\rprotocol.ps1` (login 0x64 → auth char 0x65 de
17 bytes → eco 4 bytes → lista 0x6b → crear 0xa39 → select 0x66 →
map-enter **0x0888** de 19 bytes → drain → close limpio).
Lecciones caras documentadas en el script: concatenación de arrays en
PowerShell, paquetes sin campo de longitud (eco, 0xac5 de 156 fijas),
0x72/0x363 reciclados para otros paquetes.
Resultado archivado: `C:\rathena-test\phaseProto-final.txt` (FAILURES=0,
720 bytes del map, `SockTest` persistido en `char`, 13 filas en `loginlog`).
Cubre: auth, creación, select, links inter-server, entrada a mapa,
save-on-quit, arranque con BD. Última ejecución verde íntegra.

## Pendiente de cliente real

Slot swap visual, diálogos NPC, skills, drops, mail entre jugadores,
recovery en juego (tumbar BD/map jugando), texto largo en chat, warps.
Nada de esto puede probarlo el driver socket.

## Sospecha independiente (NO bug confirmado)

Crash determinista del map-server al arrancar con caché sintética de ≥2
mapas con celdas (1 mapa OK; archivo validado byte a byte: 15 entradas,
zlib íntegro). Antes de declararlo bug de producción hay que descartarlo
con un cache generado por `mapcache` oficial + datos válidos. No bloquea
el driver (usa 1 mapa) ni el merge de nada. Requiere decisión separada.

## Estado del entorno

Todo apagado (MariaDB, login, char, map). BD `ragnarok_test` intacta en
disco (`C:\rathena-test\data`). Repo limpio salvo `.freebuff/` preexistente.
