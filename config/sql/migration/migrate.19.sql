-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE ecu_serials_migrate(id INTEGER PRIMARY KEY, serial TEXT UNIQUE, hardware_id TEXT NOT NULL, is_primary INTEGER NOT NULL DEFAULT 0 CHECK (is_primary IN (0,1)));
INSERT INTO ecu_serials_migrate(serial, hardware_id, is_primary) SELECT ecu_serials.serial, ecu_serials.hardware_id, ecu_serials.is_primary FROM ecu_serials ORDER BY is_primary DESC, ecu_serials.rowid;

DROP TABLE ecu_serials;
ALTER TABLE ecu_serials_migrate RENAME TO ecu_serials;

DELETE FROM version;
INSERT INTO version VALUES(19);

RELEASE MIGRATION;
