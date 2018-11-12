-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE need_reboot(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), flag INTEGER NOT NULL DEFAULT 0);

DELETE FROM version;
INSERT INTO version VALUES(12);

COMMIT TRANSACTION;
