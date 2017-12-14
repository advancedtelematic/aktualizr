BEGIN TRANSACTION;

ALTER TABLE primary_image ADD COLUMN installed_versions TEXT NOT NULL DEFAULT '';

CREATE TABLE device_info_migrate(device_id TEXT, is_registered INTEGER NOT NULL DEFAULT 0 CHECK (is_registered IN (0,1)));
INSERT INTO device_info_migrate(device_id, is_registered) SELECT device_id, is_registered FROM device_info LIMIT 1;
DROP TABLE device_info;
ALTER TABLE device_info_migrate RENAME TO device_info;

DELETE FROM version;
INSERT INTO version VALUES(1);

COMMIT TRANSACTION;
