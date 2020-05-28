-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

ALTER TABLE ecu_serials RENAME TO ecus;
CREATE TABLE secondary_ecus(serial TEXT PRIMARY KEY, sec_type TEXT, public_key_type TEXT, public_key TEXT, extra TEXT, manifest TEXT);

DELETE FROM version;
INSERT INTO version VALUES(23);

RELEASE MIGRATION;
