CREATE TABLE version(version INTEGER);
CREATE TABLE device_info(device_id TEXT, is_registered INTEGER NOT NULL DEFAULT 0 CHECK (is_registered IN (0,1)));
CREATE TABLE ecu_serials(serial TEXT UNIQUE, hardware_id TEXT NOT NULL, is_primary INTEGER NOT NULL CHECK (is_primary IN (0,1)));
CREATE TABLE installed_versions(hash TEXT UNIQUE, name TEXT NOT NULL);
CREATE TABLE primary_keys(private TEXT, public TEXT);
CREATE TABLE tls_creds(ca_cert BLOB, ca_cert_format TEXT,
                       client_cert BLOB, client_cert_format TEXT,
                       client_pkey BLOB, client_pkey_format TEXT);
CREATE TABLE root_meta(root BLOB NOT NULL, root_format TEXT NOT NULL, director INTEGER NOT NULL CHECK (director IN (0,1)), version INTEGER NOT NULL);
CREATE TABLE meta(director_targets BLOB NOT NULL, director_targets_format TEXT NOT NULL,
                  image_target BLOB NOT NULL, image_targets_format TEXT NOT NULL,
                  image_timestamp BLOB NOT NULL, image_timestamp_format TEXT NOT NULL,
                  image_snapshot BLOB NOT NULL, image_snapshot_format TEXT NOT NULL,
                  time BLOB NOT NULL, time_format TEXT NOT NULL);
CREATE TABLE primary_image(filepath TEXT NOT NULL, installed_versions TEXT NOT NULL DEFAULT '');
