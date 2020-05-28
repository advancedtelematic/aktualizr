-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

ALTER TABLE target_images ADD sha256 TEXT NOT NULL DEFAULT "";
ALTER TABLE target_images ADD sha512 TEXT NOT NULL DEFAULT "";

DELETE FROM version;
INSERT INTO version VALUES(11);

RELEASE MIGRATION;
