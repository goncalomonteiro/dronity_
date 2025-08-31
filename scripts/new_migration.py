#!/usr/bin/env python3
"""Create a new SQL migration file under desktop/schema/migrations.

Usage:
  python scripts/new_migration.py "add_x_table"

This will create V####__add_x_table.sql with the next numeric version.
"""
from __future__ import annotations

import re
import sys
from datetime import datetime
from pathlib import Path


def next_version(mig_dir: Path) -> int:
    versions = []
    for p in mig_dir.glob("V*.sql"):
        m = re.match(r"V(\d+)__", p.name)
        if m:
            versions.append(int(m.group(1)))
    return max(versions or [0]) + 1


def main(argv: list[str]) -> int:
    if not argv:
        print("usage: new_migration.py <title>")
        return 2
    title = argv[0].strip().replace(" ", "_")
    root = Path(__file__).resolve().parents[1]
    mig_dir = root / "desktop" / "schema" / "migrations"
    mig_dir.mkdir(parents=True, exist_ok=True)
    ver = next_version(mig_dir)
    fname = mig_dir / f"V{ver:04d}__{title}.sql"
    if fname.exists():
        print(f"migration already exists: {fname}")
        return 1
    now = datetime.utcnow().isoformat() + "Z"
    content = f"""-- Migration V{ver:04d}: {title}
-- Created {now}

-- Write SQL here. Wrap schema changes in a transaction where applicable.
-- Example:
-- BEGIN;
--   ALTER TABLE projects ADD COLUMN example INTEGER DEFAULT 0;
--   INSERT OR IGNORE INTO schema_migrations(version, applied_at) VALUES ({ver}, CAST(strftime('%s','now') AS INTEGER));
-- COMMIT;
"""
    fname.write_text(content, encoding="utf-8")
    print(str(fname))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

