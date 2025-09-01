Verity Drone Show Tool — Monorepo (Bootstrap)

This repository is a monorepo housing desktop, web, engine, backend, scripts, and ops.

Stacks
- `engine`: C++ core library (CMake) with unit tests
- `desktop`: C++ desktop app stub (CMake)
- `backend`: Python (FastAPI) service skeleton with tests
- `web`: React + TypeScript skeleton with lint/format/test/build
- `scripts`: Utility scripts (Python)
- `ops`: CI/CD and documentation (ADRs, templates)

Local Dev Quickstart
- Engine/Desktop: `cmake -S engine -B engine/build && cmake --build engine/build && cmake -S desktop -B desktop/build && cmake --build desktop/build`
- Backend: `cd backend && python -m venv .venv && . .venv/bin/activate (or .venv\\Scripts\\activate)` then `pip install -e .[dev] && pytest && ruff check . && black --check . && pip-audit`
- Web: `cd web && npm ci && npm run lint && npm run typecheck && npm test && npm run build`

Contributing & Standards
- Conventional Commits for messages; semantic version tags planned per stack
- Formatters/Linters: clang-format/clang-tidy (C++), black/ruff (Python), eslint/prettier (TS)
- See `CONTRIBUTING.md` for details

CI
- GitHub Actions builds and tests all stacks; runs static analysis and basic security scans

ADRs
- Architecture Decision Records under `ops/docs/adr`. Start new ADRs from the template.

## Step 0 — Project Bootstrap & Guardrails

What was done
- Monorepo layout: `engine/` (C++ core lib + tests), `desktop/` (C++ app stub), `backend/` (FastAPI + tests), `web/` (React/TS + tests/build), `scripts/`, `ops/`.
- Tooling guardrails: root `.editorconfig`, `.gitignore`, `.gitattributes`, `.clang-format`, `.clang-tidy`, `CONTRIBUTING.md`.
- CI pipelines: `.github/workflows/ci.yml` builds/tests/analyzes all stacks; CodeQL security scanning in `.github/workflows/codeql.yml`.
- Quality/security: C++ `clang-tidy`; Python `ruff`, `black`, `pytest`, `pip-audit`; Web `eslint`, `prettier`, `vitest`, `tsc`.
- ADRs & templates: ADR template at `ops/docs/adr/0000-template.md`; issue/PR templates under `.github/ISSUE_TEMPLATE/` and `.github/PULL_REQUEST_TEMPLATE.md`; releases guidance in `ops/docs/RELEASES.md`.
- Minimal runnable skeletons: each stack has a trivial build/test to prove wiring and enable CI DoD.

Why this step matters
- Velocity: standardized structure and commands reduce setup friction across teams.
- Quality by default: formatters/linters/tests run locally and in CI, preventing drift and regressions early.
- Reproducibility: deterministic builds and checks across C++, Python, and TS with pinned tools in CI.
- Collaboration: ADRs and templates codify decisions and streamline issue/PR flow.
- Security/compliance: CodeQL and `pip-audit` introduce early vulnerability signals; groundwork for signed releases later.
- Extensibility: clear separation of `engine`, `desktop`, `web`, and `backend` supports parallel development aligned with the blueprint.

Definition of Done (met)
- A trivial change touching all stacks builds and tests in CI, exercising compilers, unit tests, linters, and security scans.

## Run All App Variants (Quick)

- Desktop runner (NullStorage): `cmake -S desktop -B desktop/build && cmake --build desktop/build && ./desktop/build/verity_desktop_runner`
- Desktop runner (SQLite):
  - Create a project: `python scripts/sceneproj.py create MyShow.sceneproj --name "My Show"`
  - Build with SQLite: `cmake -S desktop -B desktop/build -DENABLE_SQLITE=ON && cmake --build desktop/build`
  - Run: `./desktop/build/verity_desktop_runner --db MyShow.sceneproj/project.db`
  - Restore from journal: `./desktop/build/verity_desktop_runner --db MyShow.sceneproj/project.db --restore`
- Qt shell (UI scaffold):
  - Ensure Qt6 Widgets is installed. If needed, set `CMAKE_PREFIX_PATH` to your Qt path, e.g. `C:\Qt\6.9.2\msvc2022_64` on Windows.
  - Configure: `cmake -S desktop -B desktop/build -DENABLE_QT_SHELL=ON`
  - Build: `cmake --build desktop/build`
  - Optional autosave: set `VERITY_PROJECT_DIR` to your `.sceneproj` directory before launching.
  - Run: `./desktop/build/verity_qt_shell`
- Scripts (project tool): `python scripts/sceneproj.py --help`
- Engine tests: `cmake -S engine -B engine/build && cmake --build engine/build && ctest --test-dir engine/build --output-on-failure`
  - Engine benchmark (optional): `cmake -S engine -B engine/build -DVERITY_ENGINE_BUILD_BENCH=ON && cmake --build engine/build && ./engine/build/engine_bench`
- Backend: `cd backend && pip install -e .[dev] && uvicorn app.main:app --reload`
- Web: `cd web && npm ci && npm run dev` (or `npm test` / `npm run build`)

Notes (Windows):
- Use PowerShell 7 (`pwsh`) for `&&`. In Windows PowerShell 5.1, run on separate lines or use `;`.
- If the Qt shell fails at runtime with missing DLLs, either add `C:\Qt\<ver>\msvc2022_64\bin` to `PATH` or run `windeployqt.exe` on the built `verity_qt_shell.exe`.
- If CMake says the generator/platform changed, use a new build directory or delete `desktop/build*`.

<!-- Step 3 content appears below after Step 2 to preserve order. -->

## Step 3 — Core Curve Engine (C++)

What’s implemented
- Curve evaluators: Hermite, Bezier (via Hermite‑equivalent control), Catmull‑Rom (centripetal).
- Constant‑speed option: per‑segment arc‑length LUT for steadier motion.
- Blending: linear blend between two curves at a given time.
- API: `createCurve(kind)`, `setKeys(id, keys)`, `setConstantSpeed(id, on)`, `evaluate(id, time)`, `evaluateBlended(a,b,alpha,time)`.

Build & test
- `cmake -S engine -B engine/build && cmake --build engine/build`
- `ctest --test-dir engine/build --output-on-failure`
- Optional benchmark: `cmake -S engine -B engine/build -DVERITY_ENGINE_BUILD_BENCH=ON && cmake --build engine/build && ./engine/build/engine_bench`

Usage (C++ snippet)
```
using namespace verity;
int id = createCurve(CurveKind::Hermite);
std::vector<Key> keys{{0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 0.f}};
setKeys(id, keys);
setConstantSpeed(id, true);
float v = evaluate(id, 0.5f);
```

## End‑to‑End Check (Steps 0–3)

Quick scripts
- Windows (PowerShell 7): `pwsh scripts/e2e_step3.ps1`
- Linux/macOS: `bash scripts/e2e_step3.sh`

What they do
- Build desktop (SQLite enabled), create `.sceneproj`, add scene+track, run the desktop runner to record revisions, replay with `--restore`, and verify DB sanity. Use engine tests/bench (above) to validate the curve core.

## Step 1 — Data Schema & Project Package (Desktop)

What was done
- Defined `.sceneproj/` package layout: directory with `project.db` (SQLite, WAL), and subfolders `/assets`, `/bakes`, `/thumbs`, `/snapshots`.
- Added SQLite schema and migrations:
  - `desktop/schema/schema.sql` with tables: `projects`, `scenes`, `tracks`, `keyframes`, `curves`, `assets`, `revisions`, `jobs`, `events`, and `schema_migrations`.
  - `desktop/schema/migrations/V0001__init.sql` bootstraps schema and records version.
- Implemented package IO utility:
  - `scripts/sceneproj.py` CLI to create/open a project, add scene/track/keyframe, and autosave/restore snapshots.
- Safety defaults: `PRAGMA foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`.

Usage
- Create project: `python scripts/sceneproj.py create MyShow.sceneproj --name "My Show"`
- Open/inspect: `python scripts/sceneproj.py open MyShow.sceneproj`
- Add scene: `python scripts/sceneproj.py add-scene MyShow.sceneproj --name "Act 1"`
- Add track: `python scripts/sceneproj.py add-track MyShow.sceneproj --scene-id <SCENE_UUID> --name PathA --kind curve`
- Add key: `python scripts/sceneproj.py add-key MyShow.sceneproj --track-id <TRACK_UUID> --t 1000 --value '{"x":1}' --interp auto`
- Autosave/restore: `python scripts/sceneproj.py autosave MyShow.sceneproj --slot 1` / `restore ... --slot 1`

Niceties
- JSON mode for scripting: add `--json` to any command for machine-readable output. Example: `python scripts/sceneproj.py --json open MyShow.sceneproj`.
- Deterministic IDs: `add-scene`, `add-track`, and `add-key` accept `--id <uuid>` for reproducible pipelines.
- Migration scaffolding: `python scripts/new_migration.py "add_column_x"` creates `desktop/schema/migrations/V####__add_column_x.sql` with a boilerplate.
- CI smoke test: Workflow “Desktop schema smoke” creates a `.sceneproj`, adds a scene/track/keyframe, autosaves/restores, and verifies counts in SQLite.

Why this step matters
- Establishes a durable, transactional project format aligned with the desktop editor’s needs.
- Enables early scripting and test flows before the full Qt shell arrives.
- WAL mode + foreign keys provide safety and integrity; snapshots/autosave support recovery.

Definition of Done
- You can create/open/save a project; add a scene/track/keyframe; and autosave + restore via the CLI above.

## Step 2 — Command Bus (Undo/Redo) & Desktop Shell

What was done
- Command framework (C++): `desktop/include/verity/command.hpp`, `desktop/src/command.cpp`.
  - ICommand interface, CommandStack with do/undo/redo and batching.
  - Transaction hooks via `IStorage` (with `NullStorage` default) and revision persistence hook.
- Sample commands: `AddKeyframeCommand`, `MoveSelectionCommand` under `desktop/src/commands/`.
- Optional SQLite-backed storage: `SqliteStorage` interface (`desktop/include/verity/db.hpp`, `desktop/src/db.cpp`) compiled when `ENABLE_SQLITE=ON`.
- Desktop runner: `verity_desktop_runner` exercises the command bus with sample commands.
- Optional Qt shell scaffold: `desktop/src/main_qt.cpp` (dockable Timeline/Graph/Viewport placeholders), built with `-DENABLE_QT_SHELL=ON` if Qt6 Widgets is available.

Usage
- Build desktop targets:
  - `cmake -S desktop -B desktop/build -G Ninja`
  - `cmake --build desktop/build`
  - Run: `./desktop/build/verity_desktop_runner`
- Enable SQLite integration (local dev):
  - Install SQLite dev libs, then configure with `-DENABLE_SQLITE=ON`.
  - Example: `cmake -S desktop -B desktop/build -G Ninja -DENABLE_SQLITE=ON`
- Optional Qt shell:
  - If Qt6 Widgets is installed: `cmake -S desktop -B desktop/build -G Ninja -DENABLE_QT_SHELL=ON`
  - Run: `./desktop/build/verity_qt_shell`

Why this step matters
- Ensures edits are reversible, consistent, and transaction-wrapped.
- Provides a clear API for desktop tooling and later Python bindings.
- Qt shell scaffold unblocks UI work (panels, docking) without blocking engine integration.

Definition of Done
- Commands execute with transaction hooks, support undo/redo, and persist revision stubs.
- App shell scaffold exists; future work will wire actual UI interactions to the command bus and DB.

Additions implemented now
- SQL-backed commands (optional): When built with `-DENABLE_SQLITE=ON`, `AddKeyframeCommand` inserts into `keyframes`, `MoveSelectionCommand` updates `t_ms`, and undo paths delete/restore.
- Command tests: `desktop/tests/commands_tests.cpp` validates do/undo/redo against a temp SQLite DB. CI job “Desktop (SQLite command tests)” installs `libsqlite3-dev`, builds with `-DENABLE_SQLITE=ON`, and runs CTest.
- Autosave cadence: `desktop/include/verity/autosave.hpp` provides an `AutosaveScheduler` that periodically snapshots `project.db` to `snapshots/slot1.db`.

Notes on undo stack persistence
- Revisions are recorded in the DB (`revisions` table) during command execution. A lightweight journal/restore strategy can reconstruct history on startup, and will be finalized alongside the desktop shell wiring in later steps.

## Step 3 — Core Curve Engine (C++)

What’s implemented
- Curve evaluators: Hermite, Bezier (via Hermite-equivalent control), Catmull-Rom (centripetal).
- Constant-speed option: per-segment arc-length LUT for steadier motion.
- Blending: linear blend between two curves at a given time.
- API: `createCurve(kind)`, `setKeys(id, keys)`, `setConstantSpeed(id, on)`, `evaluate(id, time)`, `evaluateBlended(a,b,alpha,time)`.

Build & test
- `cmake -S engine -B engine/build && cmake --build engine/build`
- `ctest --test-dir engine/build --output-on-failure`
- Optional benchmark: `cmake -S engine -B engine/build -DVERITY_ENGINE_BUILD_BENCH=ON && cmake --build engine/build && ./engine/build/engine_bench`

Usage (C++ snippet)
```
using namespace verity;
int id = createCurve(CurveKind::Hermite);
std::vector<Key> keys{{0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 0.f}};
setKeys(id, keys);
setConstantSpeed(id, true);
float v = evaluate(id, 0.5f);
```

## End-to-End Check (Steps 0–3)

Quick scripts
- Windows (PowerShell 7): `pwsh scripts/e2e_step3.ps1`
- Linux/macOS: `bash scripts/e2e_step3.sh`

What they do
- Build desktop (SQLite enabled), create `.sceneproj`, add scene+track, run the desktop runner to record revisions, replay with `--restore`, and verify DB sanity. Use engine tests/bench (above) to validate the curve core.
