-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

CREATE TABLE installed_versions_migrate(id INTEGER PRIMARY KEY, ecu_serial TEXT NOT NULL, sha256 TEXT NOT NULL, name TEXT NOT NULL, hashes TEXT NOT NULL, length INTEGER NOT NULL DEFAULT 0, correlation_id TEXT NOT NULL DEFAULT '', is_current INTEGER NOT NULL CHECK (is_current IN (0,1)) DEFAULT 0, is_pending INTEGER NOT NULL CHECK (is_pending IN (0,1)) DEFAULT 0, was_installed INTEGER NOT NULL CHECK (was_installed IN (0,1)) DEFAULT 0);
INSERT INTO installed_versions_migrate(ecu_serial, sha256, name, hashes, length, correlation_id, is_current, is_pending, was_installed) SELECT installed_versions.ecu_serial, installed_versions.sha256, installed_versions.name, installed_versions.hashes, installed_versions.length, installed_versions.correlation_id, installed_versions.is_current, installed_versions.is_pending, installed_versions.was_installed FROM installed_versions;

DROP TABLE installed_versions;
ALTER TABLE installed_versions_migrate RENAME TO installed_versions;

DELETE FROM version;
INSERT INTO version VALUES(20);

RELEASE ROLLBACK_MIGRATION;
