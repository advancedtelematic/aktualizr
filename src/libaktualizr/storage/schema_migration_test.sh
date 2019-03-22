#!/bin/bash

set -euo pipefail

if [ $# != 1 ]; then
    echo "usage: $0 sql_dir" >&2
    exit 1
fi

SQL_DIR=$1

DB_CUR=$(mktemp)
trap 'rm $DB_CUR' exit

DB_MIG=$(mktemp)
trap 'rm $DB_MIG' exit

if ! [ -f "$SQL_DIR/schema.sql" ]; then
    echo "schema.sql not found in $SQL_DIR"
    exit 1
fi

sqlite3 -batch -init "$SQL_DIR/schema.sql" "$DB_CUR" ";"
for f in "$SQL_DIR"/migration/migrate.*.sql; do
    R=$( { sqlite3 -batch -init "$f" "$DB_MIG" ";"; } 2>&1 )
    if [ -n "$R" ]; then
        echo $R
        exit 1
    fi
done

# ignore the internal 'sqlite_sequence' table, used to track auto-increment ids
R=$(sqldiff "$DB_CUR" "$DB_MIG" | grep -v '^INSERT INTO sqlite_sequence' || true)
if [ -z "$R" ]; then
    exit 0
fi

echo "$R"
exit 1
