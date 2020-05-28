-- Don't modify this! Create a new migration instead--see docs/ota-client-guide/modules/ROOT/pages/schema-migrations.adoc
SAVEPOINT MIGRATION;

CREATE TABLE report_events(id INTEGER PRIMARY KEY, json_string TEXT NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(24);

RELEASE MIGRATION;
