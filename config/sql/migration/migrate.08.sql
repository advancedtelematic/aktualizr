-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE primary_keys_migrate(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), private TEXT, public TEXT);
INSERT INTO primary_keys_migrate SELECT 0,private,public FROM primary_keys LIMIT 1;
DROP TABLE primary_keys;
ALTER TABLE primary_keys_migrate RENAME TO primary_keys;

CREATE TABLE device_info_migrate(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), device_id TEXT, is_registered INTEGER NOT NULL DEFAULT 0 CHECK (is_registered IN (0,1)));
INSERT INTO device_info_migrate SELECT 0,device_id,is_registered FROM device_info LIMIT 1;
DROP TABLE device_info;
ALTER TABLE device_info_migrate RENAME TO device_info;

DELETE FROM version;
INSERT INTO version VALUES(8);

RELEASE MIGRATION;
