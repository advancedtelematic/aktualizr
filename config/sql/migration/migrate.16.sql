SAVEPOINT MIGRATION;

DROP TABLE installation_result;
CREATE TABLE device_installation_result(unique_mark INTEGER PRIMARY KEY CHECK (unique_mark = 0), success INTEGER NOT NULL DEFAULT 0, result_code TEXT NOT NULL DEFAULT "", description TEXT NOT NULL DEFAULT "", raw_report TEXT NOT NULL DEFAULT "");
CREATE TABLE ecu_installation_results(ecu_serial TEXT NOT NULL PRIMARY KEY, success INTEGER NOT NULL DEFAULT 0, result_code TEXT NOT NULL DEFAULT "", description TEXT NOT NULL DEFAULT "");

DELETE FROM version;
INSERT INTO version VALUES(16);

RELEASE MIGRATION;
