CREATE TABLE version(version INTEGER);
INSERT INTO version(rowid,version) VALUES(1,23);
CREATE TABLE device_info(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), device_id TEXT, is_registered INTEGER NOT NULL DEFAULT 0 CHECK (is_registered IN (0,1)));
CREATE TABLE ecus(id INTEGER PRIMARY KEY, serial TEXT UNIQUE, hardware_id TEXT NOT NULL, is_primary INTEGER NOT NULL DEFAULT 0 CHECK (is_primary IN (0,1)));
CREATE TABLE secondary_ecus(serial TEXT PRIMARY KEY, sec_type TEXT, public_key_type TEXT, public_key TEXT, extra TEXT);
CREATE TABLE misconfigured_ecus(serial TEXT UNIQUE, hardware_id TEXT NOT NULL, state INTEGER NOT NULL CHECK (state IN (0,1)));
CREATE TABLE installed_versions(id INTEGER PRIMARY KEY, ecu_serial TEXT NOT NULL, sha256 TEXT NOT NULL, name TEXT NOT NULL, hashes TEXT NOT NULL, length INTEGER NOT NULL DEFAULT 0, correlation_id TEXT NOT NULL DEFAULT '', is_current INTEGER NOT NULL CHECK (is_current IN (0,1)) DEFAULT 0, is_pending INTEGER NOT NULL CHECK (is_pending IN (0,1)) DEFAULT 0, was_installed INTEGER NOT NULL CHECK (was_installed IN (0,1)) DEFAULT 0, custom_meta TEXT NOT NULL DEFAULT "");
CREATE TABLE primary_keys(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), private TEXT, public TEXT);
CREATE TABLE tls_creds(ca_cert BLOB, ca_cert_format TEXT,
                       client_cert BLOB, client_cert_format TEXT,
                       client_pkey BLOB, client_pkey_format TEXT);
CREATE TABLE meta(meta BLOB NOT NULL, repo INTEGER NOT NULL, meta_type INTEGER NOT NULL, version INTEGER NOT NULL, UNIQUE(repo, meta_type, version));
CREATE TABLE target_images(targetname TEXT PRIMARY KEY, real_size INTEGER NOT NULL DEFAULT 0, sha256 TEXT NOT NULL DEFAULT "", sha512 TEXT NOT NULL DEFAULT "", filename TEXT NOT NULL);
CREATE TABLE repo_types(repo INTEGER NOT NULL, repo_string TEXT NOT NULL);
CREATE TABLE meta_types(meta INTEGER NOT NULL, meta_string TEXT NOT NULL);
INSERT INTO meta_types(rowid,meta,meta_string) VALUES(1,0,'root');
INSERT INTO meta_types(rowid,meta,meta_string) VALUES(2,1,'snapshot');
INSERT INTO meta_types(rowid,meta,meta_string) VALUES(3,2,'targets');
INSERT INTO meta_types(rowid,meta,meta_string) VALUES(4,3,'timestamp');
INSERT INTO repo_types(rowid,repo,repo_string) VALUES(1,0,'images');
INSERT INTO repo_types(rowid,repo,repo_string) VALUES(2,1,'director');
CREATE TABLE device_installation_result(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), success INTEGER NOT NULL DEFAULT 0, result_code TEXT NOT NULL DEFAULT "", description TEXT NOT NULL DEFAULT "", raw_report TEXT NOT NULL DEFAULT "", correlation_id TEXT NOT NULL DEFAULT "");
CREATE TABLE ecu_installation_results(ecu_serial TEXT NOT NULL PRIMARY KEY, success INTEGER NOT NULL DEFAULT 0, result_code TEXT NOT NULL DEFAULT "", description TEXT NOT NULL DEFAULT "");
CREATE TABLE need_reboot(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), flag INTEGER NOT NULL DEFAULT 0);
CREATE TABLE rollback_migrations(version_from INT PRIMARY KEY, migration TEXT NOT NULL);
CREATE TABLE delegations(meta BLOB NOT NULL, role_name TEXT NOT NULL, UNIQUE(role_name));
CREATE TABLE ecu_report_counter(ecu_serial TEXT NOT NULL PRIMARY KEY, counter INTEGER NOT NULL DEFAULT 0);
