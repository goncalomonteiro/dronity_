-- Migration V0001: initial schema
.read schema.sql

INSERT OR IGNORE INTO schema_migrations(version, applied_at)
VALUES (1, CAST(strftime('%s','now') AS INTEGER));

