Verity Drone Show Tool — Monorepo

This repository houses the desktop app, real‑time engine, web app, backend API, scripts, and ops. Below, each step is documented in order with: activities/purpose, inputs, outputs with locations, automated tests (and when they run in CI), manual commands to run locally, and a short note on how the architecture evolved.

Stacks
- `engine`: C++ core library (CMake)
- `desktop`: C++ desktop app (runner + Qt shell)
- `backend`: FastAPI skeleton (Python)
- `web`: React + TypeScript skeleton
- `scripts`: Python utilities (project/package/migrations, e2e)
- `ops`: CI/CD and ADRs

---

## Step 0 — Project Bootstrap & Guardrails

- Activities & Purpose: Monorepo layout, formatters/linters, CI pipelines, ADRs and contribution templates to enable fast, safe iteration.
- Inputs: None (bootstrap).
- Outputs (locations):
  - Structure: `engine/`, `desktop/`, `backend/`, `web/`, `scripts/`, `ops/`.
  - Tooling: `/.clang-format`, `/.clang-tidy`, `/.editorconfig`, `.github/workflows/ci.yml`, `.github/workflows/codeql.yml`, `CONTRIBUTING.md`, `ops/docs/adr/0000-template.md`, `ops/docs/RELEASES.md`.
- Automated Tests (CI):
  - CodeQL (C++/Python/JS): “CodeQL / analyze (…)”.
  - Engine/Desktop build + tests + clang‑tidy: “CI / C++ Engine/Desktop”.
  - Backend lint/format/tests/audit: “CI / Backend (Python)”.
  - Web lint/typecheck/tests/build: “CI / Web (React/TS)”.
- Manual Run: See step‑specific commands below.
- Architecture Evolution: Establishes clean boundaries between stacks with shared quality gates; no data or UI yet.

---

## Step 1 — Data Schema & Project Package (Desktop)

- Activities & Purpose: Define `.sceneproj` package (SQLite + folders) and schema; provide CLI to create/open projects and add basic data; enable autosave/restore snapshots.
- Inputs: CLI args to `scripts/sceneproj.py`.
- Outputs (locations):
  - Schema/migrations: `desktop/schema/schema.sql`, `desktop/schema/migrations/`.
  - Package tool: `scripts/sceneproj.py` (create/open/add-scene/add-track/add-key/autosave/restore).
  - Migration scaffolder: `scripts/new_migration.py`.
- Automated Tests (CI):
  - Smoke: “CI / Desktop schema smoke” — creates a `.sceneproj`, adds rows, autosaves/restores, asserts counts.
- Manual Run (examples):
  - Create: `python scripts/sceneproj.py create MyShow.sceneproj --name "My Show"`
  - Add scene/track/key: `add-scene`, `add-track`, `add-key` (see `--help`).
  - Autosave/restore snapshot: `autosave --slot 1` / `restore --slot 1`.
- Architecture Evolution: Introduces a durable local project format shared by desktop tools.

---

## Step 2 — Command Bus (Undo/Redo) & Desktop Shell

- Activities & Purpose: Uniform, reversible edit pipeline (do/undo/redo) with DB transactions and revision persistence; Qt shell scaffold to host authoring UI.
- Inputs: Commands executed by the headless runner now; UI later.
- Outputs (locations):
  - Command framework: `desktop/include/verity/command.hpp`, `desktop/src/command.cpp` (batching, transactions, coalesced revisions).
  - Sample commands: `desktop/src/commands/add_keyframe.cpp`, `desktop/src/commands/move_selection.cpp`.
  - SQLite storage: `desktop/include/verity/db.hpp`, `desktop/src/db.cpp` (WAL, revision log, helpers).
  - Autosave scheduler: `desktop/include/verity/autosave.hpp`, `desktop/src/autosave.cpp`.
  - Qt shell: `desktop/src/main_qt.cpp` (dockable panels; status bar; File → “Save Snapshot Now”).
- Automated Tests (CI):
  - Integration: “CI / Desktop (SQLite command tests)” — do/undo/redo against real SQLite; checks revision rows.
  - Smoke: “CI / Desktop restore smoke (--restore)” — replays revisions into a fresh DB and asserts keyframes exist.
  - E2E: “CI / Desktop E2E (Steps 0–3)” — runs `scripts/e2e_step3.sh` end‑to‑end on Linux.
  - Build: “CI / Desktop (Qt shell build)” — compiles optional UI.
- Manual Run:
  - Runner (NullStorage):
    - `cmake -S desktop -B desktop/build && cmake --build desktop/build`
    - `./desktop/build/verity_desktop_runner`
  - Runner (SQLite):
    - `cmake -S desktop -B desktop/build -DENABLE_SQLITE=ON && cmake --build desktop/build`
    - `./desktop/build/verity_desktop_runner --db MyShow.sceneproj/project.db [--restore]`
  - Qt shell (optional):
    - Configure with Qt and build.
    - `VERITY_PROJECT_DIR=MyShow.sceneproj ./desktop/build/verity_qt_shell` (autosave every 60s; menu to snapshot now).
- Architecture Evolution: Adds reliable edit core + host UI; still no interactive authoring yet.

---

## Step 3 — Core Curve Engine (C++)

- Activities & Purpose: Real‑time curve evaluation, constant‑speed remap, and blending — the math core powering previews and later the viewport.
- Curve Types & Purpose:
  - Hermite: smooth interpolation with explicit tangents; animator‑friendly shaping.
  - Bezier (cubic): familiar handle‑based easing; pairs well with graph editors.
  - Catmull‑Rom (centripetal): passes through points; quick path shaping without tangent editing.
  - Constant‑speed (arc‑length LUT): evens out perceived speed along curved paths.
- Inputs: C++ API calls to `createCurve`, `setKeys`, `setConstantSpeed`, `evaluate`, `evaluateBlended`.
- Outputs (locations):
  - API/impl: `engine/include/verity/engine.hpp`, `engine/src/engine.cpp`.
  - Tests: `engine/tests/engine_tests.cpp`.
  - Microbenchmark (optional): `engine/bench/curve_bench.cpp`.
- Automated Tests (CI):
  - Unit: “CI / C++ Engine/Desktop” builds engine and runs unit tests.
  - Bench: not run in CI (machine‑dependent); run locally to spot regressions.
- Manual Run:
  - Tests: `cmake -S engine -B engine/build && cmake --build engine/build && ctest --test-dir engine/build --output-on-failure`
  - Bench: `cmake -S engine -B engine/build -DVERITY_ENGINE_BUILD_BENCH=ON && cmake --build engine/build && ./engine/build/engine_bench`
- Architecture Evolution: Introduces a fast evaluation kernel to be wired into the viewport (Step 4) and editors (Step 5); later exposed to Python tools (Step 9) and WebAssembly (Step 12).

---

## End‑to‑End Scripts

- Windows: `pwsh scripts/e2e_step3.ps1`
- Linux/macOS: `bash scripts/e2e_step3.sh`
- They build desktop (SQLite), create a `.sceneproj`, add scene/track, run the runner to record revisions, replay with `--restore`, and verify DB state.

---

## Notes & Tips
- Use PowerShell 7 (`pwsh`) on Windows for `&&` chaining; Windows PowerShell 5.1 requires separate lines or `;`.
- If the Qt shell misses DLLs on Windows, add your Qt `bin` to `PATH` or run `windeployqt` for the executable.
- If CMake complains about generator/platform changes, use a fresh build directory.

