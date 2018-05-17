-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;


DROP TABLE primary_image;

CREATE TABLE target_images(filename TEXT UNIQUE, image_data BLOB NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(2);

COMMIT TRANSACTION;
