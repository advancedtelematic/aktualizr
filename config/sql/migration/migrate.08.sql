-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE primary_keys_migrate(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), private TEXT, public TEXT);
INSERT INTO primary_keys_migrate SELECT 0,private,public FROM primary_keys LIMIT 1;
DROP TABLE primary_keys;
ALTER TABLE primary_keys_migrate RENAME TO primary_keys;

DELETE FROM version;
INSERT INTO version VALUES(8);

COMMIT TRANSACTION;
