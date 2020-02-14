-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE secondary_ecus;
ALTER TABLE ecus RENAME TO ecu_serials;

DELETE FROM version;
INSERT INTO version VALUES(22);

RELEASE ROLLBACK_MIGRATION;
