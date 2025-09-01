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

# 3) Exercise command bus + revisions
Run "./$BuildDirDesktop/Release/verity_desktop_runner.exe --db $ProjectName/project.db --proj $ProjectName"

# 4) Restore from revisions into same DB (simulates restart)
Run "./$BuildDirDesktop/Release/verity_desktop_runner.exe --db $ProjectName/project.db --restore"

# 5) Verify basic state
Run "python - << 'PY'
import sqlite3,sys
db='E2E.sceneproj/project.db'
conn=sqlite3.connect(db)
cur=conn.cursor()
assert cur.execute('select count(*) from projects').fetchone()[0]==1
assert cur.execute('select count(*) from scenes').fetchone()[0]>=1
assert cur.execute('select count(*) from tracks').fetchone()[0]>=1
assert cur.execute('select count(*) from revisions').fetchone()[0]>=1
print('OK: DB state looks sane')
PY"

Write-Host "All Step 0–3 e2e checks passed." -ForegroundColor Green

