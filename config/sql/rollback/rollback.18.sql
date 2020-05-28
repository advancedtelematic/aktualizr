-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

CREATE TABLE target_images_data(filename TEXT PRIMARY KEY, image_data BLOB NOT NULL);
DROP TABLE target_images;
CREATE TABLE target_images(filename TEXT PRIMARY KEY, real_size INTEGER NOT NULL DEFAULT 0, sha256 TEXT NOT NULL DEFAULT "", sha512 TEXT NOT NULL DEFAULT "");

DELETE FROM version;
INSERT INTO version VALUES(17);

RELEASE ROLLBACK_MIGRATION;
