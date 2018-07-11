-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE installation_result(id TEXT, result_code INTEGER NOT NULL DEFAULT 0, result_text TEXT);

DELETE FROM version;
INSERT INTO version VALUES(7);

COMMIT TRANSACTION;
