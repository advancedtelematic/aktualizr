-- Don't modify this! Create a new migration instead--see docs/schema-migrations.adoc
BEGIN TRANSACTION;

CREATE TABLE rollback_migrations(version_from INT PRIMARY KEY, migration TEXT NOT NULL);

DELETE FROM version;
INSERT INTO version VALUES(15);

COMMIT TRANSACTION;
