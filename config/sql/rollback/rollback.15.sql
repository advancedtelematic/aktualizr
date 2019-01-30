-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE rollback_migrations;

DELETE FROM version;
INSERT INTO version VALUES(14);

RELEASE ROLLBACK_MIGRATION;
