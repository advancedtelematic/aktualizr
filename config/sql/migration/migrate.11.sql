-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

ALTER TABLE target_images ADD sha256 TEXT NOT NULL DEFAULT "";
ALTER TABLE target_images ADD sha512 TEXT NOT NULL DEFAULT "";

DELETE FROM version;
INSERT INTO version VALUES(11);

COMMIT TRANSACTION;
