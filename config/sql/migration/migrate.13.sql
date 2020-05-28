-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE installed_versions_migrate(ecu_serial TEXT NOT NULL, sha256 TEXT NOT NULL, name TEXT NOT NULL, hashes TEXT NOT NULL, length INTEGER NOT NULL DEFAULT 0, correlation_id TEXT NOT NULL DEFAULT '', is_current INTEGER NOT NULL CHECK (is_current IN (0,1)) DEFAULT 0, is_pending INTEGER NOT NULL CHECK (is_pending IN (0,1)) DEFAULT 0, UNIQUE(ecu_serial, sha256, name));

INSERT INTO installed_versions_migrate SELECT ecu_serials.serial, installed_versions.hash, installed_versions.name, "", installed_versions.length, '', installed_versions.is_current, 0 FROM ecu_serials INNER JOIN installed_versions ON ecu_serials.is_primary = 1;

DROP TABLE installed_versions;
ALTER TABLE installed_versions_migrate RENAME TO installed_versions;

DELETE FROM version;
INSERT INTO version VALUES(13);

RELEASE MIGRATION;
