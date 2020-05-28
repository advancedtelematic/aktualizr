-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

DROP TABLE target_images_data;
DROP TABLE target_images;
CREATE TABLE target_images(targetname TEXT PRIMARY KEY, real_size INTEGER NOT NULL DEFAULT 0, sha256 TEXT NOT NULL DEFAULT "", sha512 TEXT NOT NULL DEFAULT "", filename TEXT NOT NULL);


DELETE FROM version;
INSERT INTO version VALUES(18);

RELEASE MIGRATION;
