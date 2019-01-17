-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE delegations(meta BLOB NOT NULL, role_name TEXT NOT NULL, UNIQUE(role_name));

DELETE FROM version;
INSERT INTO version VALUES(15);

COMMIT TRANSACTION;
