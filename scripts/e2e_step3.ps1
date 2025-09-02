# End-to-end smoke for Steps 0–3 (Windows/PowerShell 7)
Param(
  [string]$ProjectName = "E2E.sceneproj",
  [string]$BuildDirDesktop = "desktop/build-msvc",
  [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'

function Run($cmd) {
  Write-Host "==> $cmd" -ForegroundColor Cyan
  Invoke-Expression $cmd
}

# 1) Ensure desktop binaries exist
if ($Rebuild -or -not (Test-Path "$BuildDirDesktop/Release/verity_desktop_runner.exe")) {
  Run 'cmake -S desktop -B desktop/build-msvc -G "Visual Studio 17 2022" -A x64 -DENABLE_SQLITE=ON -DENABLE_QT_SHELL=ON'
  Run 'cmake --build desktop/build-msvc --config Release'
}

# 2) Create project + seed scene/track ids matching runner defaults
Run "python scripts/sceneproj.py create $ProjectName --name 'E2E'"
$SCENE='11111111-1111-1111-1111-111111111111'
$TRACK='track-demo'
Run "python scripts/sceneproj.py add-scene $ProjectName --name 'Act 1' --id $SCENE"
Run "python scripts/sceneproj.py add-track $ProjectName --scene-id $SCENE --name PathA --kind curve --id $TRACK"

# 3) Exercise command bus + revisions on original DB
Run "./$BuildDirDesktop/Release/verity_desktop_runner.exe --db $ProjectName/project.db --proj $ProjectName"

# 4) Restore into a fresh DB (avoid PK conflicts) and verify
$fresh = "fresh.db"
if (Test-Path $fresh) { Remove-Item $fresh -Force }
sqlite3 $fresh @'
PRAGMA foreign_keys=ON;
CREATE TABLE projects(id TEXT PRIMARY KEY, name TEXT, version INTEGER, created_at INTEGER, updated_at INTEGER);
CREATE TABLE scenes(id TEXT PRIMARY KEY, project_id TEXT, name TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE tracks(id TEXT PRIMARY KEY, scene_id TEXT, name TEXT, kind TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE keyframes(id TEXT PRIMARY KEY, track_id TEXT NOT NULL, t_ms INTEGER NOT NULL, value_json TEXT NOT NULL, interp TEXT NOT NULL, created_at INTEGER, updated_at INTEGER);
CREATE TABLE revisions(id INTEGER PRIMARY KEY AUTOINCREMENT, project_id TEXT, user TEXT, label TEXT, diff_json TEXT, created_at INTEGER);
'@

$projId = sqlite3 "$ProjectName/project.db" "SELECT id FROM projects LIMIT 1;"
sqlite3 $fresh "INSERT INTO projects(id,name,version,created_at,updated_at) VALUES('$projId','E2E',1,0,0);"
sqlite3 $fresh "INSERT INTO scenes(id,project_id,name,created_at,updated_at) VALUES('$SCENE','$projId','Act 1',0,0);"
sqlite3 $fresh "INSERT INTO tracks(id,scene_id,name,kind,created_at,updated_at) VALUES('$TRACK','$SCENE','PathA','curve',0,0);"
sqlite3 $fresh "ATTACH '$ProjectName/project.db' AS src; INSERT INTO revisions(project_id,user,label,diff_json,created_at) SELECT project_id,user,label,diff_json,created_at FROM src.revisions; DETACH src;"

Run "./$BuildDirDesktop/Release/verity_desktop_runner.exe --db $fresh --restore"

Run "python - << 'PY'
import sqlite3
conn=sqlite3.connect('fresh.db')
cur=conn.cursor()
assert cur.execute('select count(*) from keyframes').fetchone()[0] >= 1
print('OK: restore created keyframes in fresh DB')
conn.close()
PY"

Write-Host "All Step 0–3 e2e checks passed." -ForegroundColor Green
