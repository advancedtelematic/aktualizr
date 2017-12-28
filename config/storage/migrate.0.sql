BEGIN TRANSACTION;

CREATE TABLE version(version INTEGER);

CREATE TABLE device_info(device_id TEXT NOT NULL, is_registered INTEGER NOT NULL CHECK (is_registered IN (0,1)));
CREATE TABLE ecu_serials(serial TEXT UNIQUE, hardware_id TEXT NOT NULL, is_primary INTEGER NOT NULL CHECK (is_primary IN (0,1)));
CREATE TABLE primary_keys(private TEXT, public TEXT);
CREATE TABLE installed_versions(hash TEXT UNIQUE, name TEXT NOT NULL);

CREATE TABLE tls_creds(ca_cert BLOB NOT NULL, ca_cert_format TEXT NOT NULL,
                       client_cert BLOB NOT NULL, client_cert_format TEXT NOT NULL,
                       client_pkey BLOB NOT NULL, client_pkey_format TEXT NOT NULL);
CREATE TABLE root_meta(root BLOB NOT NULL, root_format TEXT NOT NULL, director INTEGER NOT NULL CHECK (director IN (0,1)), version INTEGER NOT NULL);
CREATE TABLE meta(director_root BLOB NOT NULL,
                  director_targets BLOB NOT NULL,
                  image_root BLOB NOT NULL,
                  image_targets BLOB NOT NULL,
                  image_timestamp BLOB NOT NULL,
                  image_snapshot BLOB NOT NULL);
CREATE TABLE primary_image(filepath TEXT NOT NULL);

INSERT INTO version VALUES(0);
COMMIT TRANSACTION;
