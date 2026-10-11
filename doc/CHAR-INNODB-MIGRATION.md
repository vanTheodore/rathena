# Migración `char` MyISAM → InnoDB (opt-in, reversible)

Alcance: solo la tabla `char`. Nada más cambia (sin FKs nuevas, sin código,
sin protocolo). Las instalaciones nuevas ya la crean InnoDB (`main.sql`).

## Por qué

El intercambio de slots (`char_clif`, triangle exchange) es un único
`UPDATE ... CASE ... WHERE char_id IN (...)` con `START TRANSACTION`/`COMMIT`
alrededor. Sobre MyISAM el `ROLLBACK` se ignora en silencio (el statement
mantiene el write lock de tabla, pero un corte a mitad puede dejar escritura
parcial). Con InnoDB el rollback es real. El resto de
sentencias (autocommit) se comporta igual en ambos motores.

## Precondiciones del operador

- MariaDB ≥ 10.2 / MySQL ≥ 5.7 (InnoDB por defecto desde 5.5; sin FKs ni
  FULLTEXT en `char`, nada más que verificar).
- Ventana de mantenimiento breve: el `ALTER` copia la tabla con lock de
  escritura. Medir antes: `SELECT COUNT(*) FROM char;` (tablas de
  personajes: cientos/miles de filas → segundos).
- Backup: `mariadb-dump <db> char > char-backup.sql` (o backup completo).

## Ejecución

```sql
-- 1. Verificar motor y filas antes
SELECT ENGINE, TABLE_ROWS FROM information_schema.tables
  WHERE table_schema = DATABASE() AND table_name = 'char';
-- 2. Convertir (una sola sentencia; no requiere parar los servidores más
--    que la propia ventana del ALTER)
SOURCE sql-files/upgrades/upgrade_20261010.sql;
-- 3. Verificar después: ENGINE=InnoDB, mismas filas e índices,
--    AUTO_INCREMENT conservado
SHOW CREATE TABLE `char`;
```

## Comprobación posterior

- `SELECT ENGINE ...` = InnoDB; `COUNT(*)` idéntico; `AUTO_INCREMENT`
  idéntico (`SHOW TABLE STATUS`).
- Boot de login+char+map + crear/seleccionar/entrar/salir un personaje.
- Prueba transaccional (desechable): `START TRANSACTION; UPDATE ...;
  ROLLBACK;` debe dejar la tabla intacta (en MyISAM dejaba el cambio).

## Recuperación (rollback del operador)

```sql
ALTER TABLE `char` ENGINE = MyISAM;
```

más restaurar `char-backup.sql` si hiciera falta. No hay cambio de código
asociado: volver atrás es solo el ALTER inverso.

## Lo que NO hace esta migración

- No da atomicidad a guardados sin `START TRANSACTION` (inventario, skills:
  van por prepared statements o sentencias sueltas, igual que antes).
- No cambia concurrencia observable: un solo escritor (char-server
  single-thread); InnoDB solo elimina locks de tabla innecesarios.
- No toca `login`, regs, guilds ni mail (mail ya era InnoDB).
