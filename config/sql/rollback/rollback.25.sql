-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE device_data;

DELETE FROM version;
INSERT INTO version VALUES(24);

RELEASE ROLLBACK_MIGRATION;
