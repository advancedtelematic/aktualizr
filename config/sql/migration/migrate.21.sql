-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

ALTER TABLE installed_versions ADD COLUMN custom_meta TEXT NOT NULL DEFAULT "";

DELETE FROM version;
INSERT INTO version VALUES(21);

RELEASE MIGRATION;
