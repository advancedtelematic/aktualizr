-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

ALTER TABLE target_images ADD real_size INTEGER NOT NULL DEFAULT 0;

DELETE FROM version;
INSERT INTO version VALUES(10);

COMMIT TRANSACTION;
