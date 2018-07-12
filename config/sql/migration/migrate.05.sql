BEGIN TRANSACTION;

CREATE TABLE rawmeta(director_root BLOB NOT NULL,
                  director_targets BLOB NOT NULL,
                  image_root BLOB NOT NULL,
                  image_targets BLOB NOT NULL,
                  image_timestamp BLOB NOT NULL,
                  image_snapshot BLOB NOT NULL);

-- Nothing was stored it root_meta so far, so just drop
DROP TABLE root_meta;
CREATE TABLE root_meta(root BLOB NOT NULL, director INTEGER NOT NULL CHECK (director IN (0,1)), version INTEGER NOT NULL);
CREATE TABLE root_rawmeta(root BLOB NOT NULL, director INTEGER NOT NULL CHECK (director IN (0,1)), version INTEGER NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(5);

COMMIT TRANSACTION;
