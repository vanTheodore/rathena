-- charslot-swap-check: regression test for the single-statement char-slot
-- swap (src/char/char_clif.cpp, triangle exchange). Verifies the exact
-- UPDATE ... CASE statement shape the server sends.
--
-- Run by hand against a DISPOSABLE database (never production data), on
-- both engines (MyISAM = production default, InnoDB = control):
--   mariadb -h127.0.0.1 -P3307 -uroot -p < tools/charslot-swap-check.sql
-- Every check row must read PASS. Any FAIL line means the statement shape
-- regressed.
--
-- What this proves (and does not prove): the statement swaps exactly the
-- two intended rows, leaves others untouched, tolerates a missing row the
-- same way the old two-UPDATE form did, and re-applying the same mapping
-- is idempotent (safe under transport retry). It does NOT prove crash
-- atomicity under MyISAM: killing the server mid-statement can still leave
-- a partial write, same as any single MyISAM statement.

-- The statement shape under test (from=0, to=1, chars 101/102):
-- UPDATE `slot` SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '102' THEN '0' END WHERE `char_id` IN ('101','102')

DROP DATABASE IF EXISTS swapcheck_myisam;
DROP DATABASE IF EXISTS swapcheck_innodb;
CREATE DATABASE swapcheck_myisam;
CREATE DATABASE swapcheck_innodb;
CREATE TABLE swapcheck_myisam.slot (char_id INT UNSIGNED NOT NULL, char_num INT NOT NULL, PRIMARY KEY (char_id)) ENGINE=MyISAM;
CREATE TABLE swapcheck_innodb.slot (char_id INT UNSIGNED NOT NULL, char_num INT NOT NULL, PRIMARY KEY (char_id)) ENGINE=InnoDB;
INSERT INTO swapcheck_myisam.slot VALUES (101,0),(102,1),(103,5);
INSERT INTO swapcheck_innodb.slot VALUES (101,0),(102,1),(103,5);

-- 1. Normal swap on both engines.
UPDATE swapcheck_myisam.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '102' THEN '0' END WHERE `char_id` IN ('101','102');
UPDATE swapcheck_innodb.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '102' THEN '0' END WHERE `char_id` IN ('101','102');
SELECT 'swap-myisam' AS t, IF((SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=101)=1 AND (SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=102)=0, 'PASS', 'FAIL') AS r;
SELECT 'swap-innodb' AS t, IF((SELECT char_num FROM swapcheck_innodb.slot WHERE char_id=101)=1 AND (SELECT char_num FROM swapcheck_innodb.slot WHERE char_id=102)=0, 'PASS', 'FAIL') AS r;
SELECT 'other-untouched' AS t, IF((SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=103)=5 AND (SELECT char_num FROM swapcheck_innodb.slot WHERE char_id=103)=5, 'PASS', 'FAIL') AS r;

-- 2. Inverse swap restores the original placement.
UPDATE swapcheck_myisam.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '0' WHEN '102' THEN '1' END WHERE `char_id` IN ('101','102');
SELECT 'inverse' AS t, IF((SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=101)=0 AND (SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=102)=1, 'PASS', 'FAIL') AS r;

-- 3. Missing row: the present row still swaps (same as the old form, whose
-- first UPDATE simply matched nothing), no error, no other rows touched.
UPDATE swapcheck_myisam.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '999' THEN '0' END WHERE `char_id` IN ('101','999');
SELECT 'missing-row' AS t, IF((SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=101)=1 AND (SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=102)=1 AND (SELECT char_num FROM swapcheck_myisam.slot WHERE char_id=103)=5, 'PASS', 'FAIL') AS r;

-- 4. Re-applying the same fixed mapping is idempotent (retry-safe).
UPDATE swapcheck_innodb.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '102' THEN '0' END WHERE `char_id` IN ('101','102');
UPDATE swapcheck_innodb.slot SET `char_num`=CASE `char_id` WHEN '101' THEN '1' WHEN '102' THEN '0' END WHERE `char_id` IN ('101','102');
SELECT 'double-apply' AS t, IF((SELECT char_num FROM swapcheck_innodb.slot WHERE char_id=101)=1 AND (SELECT char_num FROM swapcheck_innodb.slot WHERE char_id=102)=0, 'PASS', 'FAIL') AS r;

DROP DATABASE IF EXISTS swapcheck_myisam;
DROP DATABASE IF EXISTS swapcheck_innodb;
SELECT 'DONE' AS t;
