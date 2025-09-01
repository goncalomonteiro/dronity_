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

# 3) Exercise command bus + revisions
"$DESKTOP_BUILD_DIR/verity_desktop_runner" --db "$PROJECT/project.db" --proj "$PROJECT"

# 4) Restore from revisions
"$DESKTOP_BUILD_DIR/verity_desktop_runner" --db "$PROJECT/project.db" --restore

# 5) Verify basic state
python - <<'PY'
import sqlite3
conn = sqlite3.connect('E2E.sceneproj/project.db')
cur = conn.cursor()
assert cur.execute('select count(*) from projects').fetchone()[0]==1
assert cur.execute('select count(*) from scenes').fetchone()[0]>=1
assert cur.execute('select count(*) from tracks').fetchone()[0]>=1
assert cur.execute('select count(*) from revisions').fetchone()[0]>=1
print('OK: DB state looks sane')
PY

echo "All Step 0–3 e2e checks passed."
