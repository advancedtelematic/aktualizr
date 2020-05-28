-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE rollback_migrations(version_from INT PRIMARY KEY, migration TEXT NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(15);

RELEASE MIGRATION;
