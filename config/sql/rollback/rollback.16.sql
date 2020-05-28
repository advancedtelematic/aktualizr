-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE device_installation_result;
DROP TABLE ecu_installation_results;
CREATE TABLE installation_result(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), id TEXT, result_code INTEGER NOT NULL DEFAULT 0, result_text TEXT);

DELETE FROM version;
INSERT INTO version VALUES(15);

RELEASE ROLLBACK_MIGRATION;
