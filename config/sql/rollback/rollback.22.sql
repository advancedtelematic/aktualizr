-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
SAVEPOINT ROLLBACK_MIGRATION;

DROP TABLE ecu_report_counter;

DELETE FROM version;
INSERT INTO version VALUES(21);

RELEASE ROLLBACK_MIGRATION;
