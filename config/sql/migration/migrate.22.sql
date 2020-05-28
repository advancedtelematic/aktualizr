-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE ecu_report_counter(ecu_serial TEXT NOT NULL PRIMARY KEY, counter INTEGER NOT NULL DEFAULT 0);

DELETE FROM version;
INSERT INTO version VALUES(22);

RELEASE MIGRATION;
