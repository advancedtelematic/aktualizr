-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE installed_versions_migrate(hash TEXT, name TEXT NOT NULL, is_current INTEGER NOT NULL CHECK (is_current IN (0,1)) DEFAULT 0, length INTEGER NOT NULL DEFAULT 0, UNIQUE(hash, name));
INSERT INTO installed_versions_migrate SELECT * FROM installed_versions;
DROP TABLE installed_versions;
ALTER TABLE installed_versions_migrate RENAME TO installed_versions;

DELETE FROM version;
INSERT INTO version VALUES(9);

RELEASE MIGRATION;
