-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

CREATE TABLE ecu_serials_migrate(serial TEXT UNIQUE, hardware_id TEXT NOT NULL, is_primary INTEGER NOT NULL CHECK (is_primary IN (0,1)));
INSERT INTO ecu_serials_migrate(serial, hardware_id, is_primary) SELECT ecu_serials.serial, ecu_serials.hardware_id, ecu_serials.is_primary FROM ecu_serials ORDER BY ecu_serials.id;

DROP TABLE ecu_serials;
ALTER TABLE ecu_serials_migrate RENAME TO ecu_serials;

DELETE FROM version;
INSERT INTO version VALUES(18);

RELEASE ROLLBACK_MIGRATION;
