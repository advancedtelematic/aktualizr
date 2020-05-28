-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE delegations(meta BLOB NOT NULL, role_name TEXT NOT NULL, UNIQUE(role_name));

DELETE FROM version;
INSERT INTO version VALUES(17);

RELEASE MIGRATION;
