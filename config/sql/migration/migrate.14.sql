-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE target_images_data(filename TEXT PRIMARY KEY, image_data BLOB NOT NULL);
CREATE TABLE target_images_info(filename TEXT PRIMARY KEY, real_size INTEGER NOT NULL DEFAULT 0, sha256 TEXT NOT NULL DEFAULT "", sha512 TEXT NOT NULL DEFAULT "");

INSERT INTO target_images_info (filename, real_size, sha256, sha512)
SELECT filename, real_size, sha256, sha512 FROM target_images;

INSERT INTO target_images_data(filename, image_data)
SELECT filename, image_data FROM target_images;

DROP TABLE target_images;
ALTER TABLE target_images_info RENAME TO target_images;

DELETE FROM version;
INSERT INTO version VALUES(14);

RELEASE MIGRATION;
