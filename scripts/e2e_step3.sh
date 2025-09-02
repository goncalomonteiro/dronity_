#!/usr/bin/env bash
set -euo pipefail

PROJECT=${1:-E2E.sceneproj}
DESKTOP_BUILD_DIR=${DESKTOP_BUILD_DIR:-desktop/build}

# 1) Ensure desktop binaries exist
if [[ ! -f "$DESKTOP_BUILD_DIR/verity_desktop_runner" ]]; then
  cmake -S desktop -B "$DESKTOP_BUILD_DIR" -G Ninja -DENABLE_SQLITE=ON
  cmake --build "$DESKTOP_BUILD_DIR" --config Release
fi

# 2) Create project + seed scene/track ids matching runner defaults
python scripts/sceneproj.py create "$PROJECT" --name "E2E"
SCENE=11111111-1111-1111-1111-111111111111
TRACK=track-demo
python scripts/sceneproj.py add-scene "$PROJECT" --name "Act 1" --id "$SCENE"
python scripts/sceneproj.py add-track "$PROJECT" --scene-id "$SCENE" --name PathA --kind curve --id "$TRACK"

# 3) Exercise command bus + revisions on original DB
"$DESKTOP_BUILD_DIR/verity_desktop_runner" --db "$PROJECT/project.db" --proj "$PROJECT"

# 4) Create fresh DB, copy minimal rows + revisions, and restore there
FRESH=fresh.db
rm -f "$FRESH"
sqlite3 "$FRESH" <<'SQL'
PRAGMA foreign_keys=ON;
CREATE TABLE projects(id TEXT PRIMARY KEY, name TEXT, version INTEGER, created_at INTEGER, updated_at INTEGER);
CREATE TABLE scenes(id TEXT PRIMARY KEY, project_id TEXT, name TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE tracks(id TEXT PRIMARY KEY, scene_id TEXT, name TEXT, kind TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE keyframes(id TEXT PRIMARY KEY, track_id TEXT NOT NULL, t_ms INTEGER NOT NULL, value_json TEXT NOT NULL, interp TEXT NOT NULL, created_at INTEGER, updated_at INTEGER);
CREATE TABLE revisions(id INTEGER PRIMARY KEY AUTOINCREMENT, project_id TEXT, user TEXT, label TEXT, diff_json TEXT, created_at INTEGER);
SQL

# Seed rows to satisfy FKs
PROJ_ID=$(sqlite3 "$PROJECT/project.db" "SELECT id FROM projects LIMIT 1;")
sqlite3 "$FRESH" "INSERT INTO projects(id,name,version,created_at,updated_at) VALUES('$PROJ_ID','E2E',1,0,0);"
sqlite3 "$FRESH" "INSERT INTO scenes(id,project_id,name,created_at,updated_at) VALUES('$SCENE','$PROJ_ID','Act 1',0,0);"
sqlite3 "$FRESH" "INSERT INTO tracks(id,scene_id,name,kind,created_at,updated_at) VALUES('$TRACK','$SCENE','PathA','curve',0,0);"

# Copy revisions from original DB
sqlite3 "$FRESH" "ATTACH '$PROJECT/project.db' AS src; INSERT INTO revisions(project_id,user,label,diff_json,created_at) SELECT project_id,user,label,diff_json,created_at FROM src.revisions; DETACH src;"

# Restore from revisions into the fresh DB
"$DESKTOP_BUILD_DIR/verity_desktop_runner" --db "$FRESH" --restore

# 5) Verify basic state
python - <<'PY'
import sqlite3
conn = sqlite3.connect('fresh.db')
cur = conn.cursor()
assert cur.execute('select count(*) from keyframes').fetchone()[0] >= 1
print('OK: restore created keyframes in fresh DB')
conn.close()
PY

echo "All Step 0–3 e2e checks passed."
