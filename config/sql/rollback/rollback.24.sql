-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE report_events;

DELETE FROM version;
INSERT INTO version VALUES(23);

RELEASE ROLLBACK_MIGRATION;
