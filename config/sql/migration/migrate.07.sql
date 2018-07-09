-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE installation_result(result TEXT);

DELETE FROM version;
INSERT INTO version VALUES(7);

COMMIT TRANSACTION;
