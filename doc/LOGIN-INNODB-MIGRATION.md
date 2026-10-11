# Migración `login` + `global_acc_reg_*` MyISAM → InnoDB (opt-in, reversible)

Alcance: exactamente tres tablas (`login`, `global_acc_reg_num`,
`global_acc_reg_str`). Nada más cambia (sin FKs nuevas, sin triggers, sin
código, sin protocolo). Las instalaciones nuevas ya las crean InnoDB
(`main.sql`); las existentes usan `sql-files/upgrades/upgrade_20261011.sql`.

## Qué atomicidad se obtiene (y qué no)

Distinguir tres cosas que no son la misma:

1. **Guardado de cuenta/token — flujo vivo, mejora real.** Cada login guarda
   la fila (`logincount`, `lastlogin`, token…) en transacción explícita sobre
   un solo handle. Sobre MyISAM el `ROLLBACK` se ignora; con InnoDB un corte
   a mitad revierte la fila en vez de dejarla parcial.
2. **Borrado en tres tablas — garantía latente.** El borrado corre en
   transacción sobre exactamente estas tres tablas, pero hoy ningún flujo del
   servidor lo invoca (sin trigger programable). La migración le da rollback
   completo *si alguna vez se usa*; no crea el trigger.
3. **Guardado de registros globales — sigue autocommit.** Corre sentencia por
   sentencia sin transacción; InnoDB no le da atomicidad multisentencia.
   Tampoco toca `acc_reg_*` (sin escritores), ni chars, inventarios, guilds,
   mails o `loginlog` de la cuenta (otros dueños, huérfanos por diseño).

## Precondiciones del operador

- MariaDB ≥ 10.2 / MySQL ≥ 5.7 (sin FKs ni FULLTEXT en estas tablas).
- Ventana de mantenimiento: cada `ALTER` copia su tabla con lock de
  escritura. Medir antes: `SELECT COUNT(*) FROM login;` (cientos/miles de
  filas → segundos; `login` suele ser la mayor de las tres).
- Backup: `mariadb-dump <db> login global_acc_reg_num global_acc_reg_str
  > login-backup.sql` (o backup completo).

## Ejecución

```sql
-- 1. Verificar motores y filas antes
SELECT table_name, ENGINE, TABLE_ROWS FROM information_schema.tables
  WHERE table_schema = DATABASE()
  AND table_name IN ('login','global_acc_reg_num','global_acc_reg_str');
-- 2. Convertir (tres sentencias independientes; no requiere parar los
--    servidores más que la propia ventana de cada ALTER)
SOURCE sql-files/upgrades/upgrade_20261011.sql;
-- 3. Verificar después: ENGINE=InnoDB en las tres, mismas filas e índices,
--    AUTO_INCREMENT de `login` conservado
SHOW CREATE TABLE `login`;
```

## Comprobación posterior

- `ENGINE` + `COUNT(*)` + `AUTO_INCREMENT` idénticos (`SHOW TABLE STATUS`).
- Boot de login+char+map y un login/logout completo (ejercita el guardado).
- Prueba transaccional (desechable): `START TRANSACTION; DELETE/UPDATE;
  ROLLBACK;` debe dejar las tablas intactas (en MyISAM dejaba el cambio).

## Recuperación (rollback del operador)

```sql
ALTER TABLE `login` ENGINE = MyISAM;
ALTER TABLE `global_acc_reg_num` ENGINE = MyISAM;
ALTER TABLE `global_acc_reg_str` ENGINE = MyISAM;
```

más restaurar `login-backup.sql` si hiciera falta. Sin cambio de código
asociado: volver atrás es solo el ALTER inverso.
