-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE target_images_data(image_data BLOB NOT NULL);
CREATE TABLE target_images_info(filename TEXT UNIQUE, real_size INTEGER NOT NULL DEFAULT 0, sha256 TEXT NOT NULL DEFAULT "", sha512 TEXT NOT NULL DEFAULT "", image_id integer NOT NULL,
        FOREIGN KEY (image_id) REFERENCES target_images_data(rowid) ON DELETE CASCADE
);

INSERT INTO target_images_info (filename, real_size, sha256, sha512, image_id)
SELECT filename, real_size, sha256, sha512, rowid FROM target_images;

INSERT INTO target_images_data(rowid, image_data)
SELECT rowid, image_data FROM target_images;

DROP TABLE target_images;
ALTER TABLE target_images_info RENAME TO target_images;

DELETE FROM version;
INSERT INTO version VALUES(14);

COMMIT TRANSACTION;
