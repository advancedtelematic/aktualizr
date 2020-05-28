-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

ALTER TABLE installed_versions RENAME TO installed_versions_old;
CREATE TABLE installed_versions(id INTEGER PRIMARY KEY, ecu_serial TEXT NOT NULL, sha256 TEXT NOT NULL, name TEXT NOT NULL, hashes TEXT NOT NULL, length INTEGER NOT NULL DEFAULT 0, correlation_id TEXT NOT NULL DEFAULT '', is_current INTEGER NOT NULL CHECK (is_current IN (0,1)) DEFAULT 0, is_pending INTEGER NOT NULL CHECK (is_pending IN (0,1)) DEFAULT 0, was_installed INTEGER NOT NULL CHECK (was_installed IN (0,1)) DEFAULT 0);
INSERT INTO installed_versions(ecu_serial, sha256, name, hashes, length, correlation_id, is_current, is_pending, was_installed) SELECT installed_versions_old.ecu_serial, installed_versions_old.sha256, installed_versions_old.name, installed_versions_old.hashes, installed_versions_old.length, installed_versions_old.correlation_id, installed_versions_old.is_current, installed_versions_old.is_pending, 1 FROM installed_versions_old ORDER BY rowid;

DROP TABLE installed_versions_old;

DELETE FROM version;
INSERT INTO version VALUES(20);

RELEASE MIGRATION;
