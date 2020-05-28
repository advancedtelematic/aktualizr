-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;


DROP TABLE primary_image;

CREATE TABLE target_images(filename TEXT UNIQUE, image_data BLOB NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(2);

RELEASE MIGRATION;
