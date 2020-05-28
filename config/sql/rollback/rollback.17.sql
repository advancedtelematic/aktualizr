-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE delegations;

DELETE FROM version;
INSERT INTO version VALUES(16);

RELEASE ROLLBACK_MIGRATION;
