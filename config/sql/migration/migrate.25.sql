-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE device_data(data_type TEXT PRIMARY KEY, hash TEXT NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(25);

RELEASE MIGRATION;
