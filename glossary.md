# Verity Glossary (by Step)

This glossary explains the concepts introduced in each step. For every term you’ll find:
- Simple terms: a plain description
- Technical terms: a precise definition
- In‑project example: how we use it here

---

## Step 0 — Bootstrap & Guardrails

- CMake
  - Simple: A builder that turns source code into programs/libraries.
  - Technical: Cross‑platform meta‑build system generating native build files (Ninja/MSBuild/Makefiles).
  - Example: `engine/CMakeLists.txt` builds `verity_engine`; `desktop/CMakeLists.txt` builds `verity_desktop_runner`.

- C++
  - Simple: The language we use for the high‑performance parts.
  - Technical: Compiled, statically‑typed language used for the engine/desktop core.
  - Example: Engine API in `engine/include/verity/engine.hpp` and implementation in `engine/src/engine.cpp`.

- Qt (Qt6 Widgets)
  - Simple: Toolkit to make desktop windows and panels.
  - Technical: Cross‑platform GUI framework; we use the Widgets module for the shell UI.
  - Example: `desktop/src/main_qt.cpp` builds the dockable shell when `-DENABLE_QT_SHELL=ON`.

- FastAPI
  - Simple: Python tool to build web APIs quickly.
  - Technical: ASGI framework for building typed REST services.
  - Example: Health endpoint in `backend/src/app/main.py` with tests in `backend/tests/test_health.py`.

- React + TypeScript
  - Simple: Web UI framework with typed JavaScript.
  - Technical: Component model (React) with static typing (TypeScript).
  - Example: Web stub in `web/src/main.tsx` and test in `web/tests/hello.test.ts`.

- Monorepo
  - Simple: One git repo for many apps/libraries.
  - Technical: Single repository that hosts multiple related projects sharing tooling/CI.
  - Example: `engine/`, `desktop/`, `backend/`, `web/`, `scripts/`, `ops/` in one repo.

- CI/CD (GitHub Actions)
  - Simple: Robots that build/test code on every change.
  - Technical: Hosted pipelines triggered on push/PR; run builds, tests, linters, security scans.
  - Example: `.github/workflows/ci.yml` runs engine/desktop/backend/web jobs.

- CodeQL
  - Simple: Security checker for code.
  - Technical: Static analysis that queries code semantics to find vulnerabilities.
  - Example: `.github/workflows/codeql.yml` analyses C++/Python/JS.

- clang-format / clang-tidy
  - Simple: Auto‑formatter and code checker for C++.
  - Technical: Formatting (style) and static analysis (lint) tools for C/C++.
  - Example: Config at `/.clang-format`, `/.clang-tidy`; called in CI for engine/desktop.

- Black / Ruff
  - Simple: Auto‑formatter and linter for Python.
  - Technical: Opinionated formatter (Black) and fast linter (Ruff) enforcing style and rules.
  - Example: Backend and `scripts/` checked in CI.

- ESLint / Prettier
  - Simple: JavaScript/TypeScript code checker and formatter.
  - Technical: Lint rules (ESLint) + formatting (Prettier) for consistent web code.
  - Example: `web/package.json` scripts run them in CI.

- ADR (Architecture Decision Record)
  - Simple: A short note that records why we chose a design.
  - Technical: Lightweight document capturing context, decision, and consequences.
  - Example: Template in `ops/docs/adr/0000-template.md`.

---

## Step 1 — Data Schema & Project Package

- Project (.sceneproj)
  - Simple: A folder for a show, with a database inside.
  - Technical: Directory package with `project.db` (SQLite) and subfolders like `/assets`, `/bakes`, `/thumbs`, `/snapshots`.
  - Example: Created via `scripts/sceneproj.py create MyShow.sceneproj`.

- SQLite
  - Simple: A file‑based database engine.
  - Technical: Embedded SQL database stored in a single file; supports transactions and WAL.
  - Example: Schema in `desktop/schema/schema.sql`.

- WAL mode
  - Simple: Write changes to a log first for safety/speed.
  - Technical: Write‑Ahead Logging improves concurrency; writers append to `-wal` while readers see a consistent snapshot.
  - Example: Enabled via `PRAGMA journal_mode=WAL;` in schema and code.

- Schema / Migration
  - Simple: The database blueprint, and step‑by‑step changes to it.
  - Technical: Table/column definitions; versioned SQL scripts to evolve structure safely.
  - Example: `desktop/schema/migrations/V0001__init.sql`; scaffolder `scripts/new_migration.py`.

- Scene
  - Simple: A chapter of the show.
  - Technical: Row in `scenes` linked to a project; contains tracks.
  - Example: `add-scene MyShow.sceneproj --name "Act 1"`.

- Track
  - Simple: A timeline lane (e.g., a drone path).
  - Technical: Row in `tracks` linked to a scene; holds keyframes/curves.
  - Example: `add-track --scene-id <SCENE_UUID> --name PathA --kind curve`.

- Curve
  - Simple: How values change smoothly between points.
  - Technical: Interpolator (Hermite/Bezier/Catmull‑Rom) used to evaluate values over time.
  - Example: Evaluated by engine at Step 3; stored as keys + type in DB.

- Keyframe (Key)
  - Simple: A point on the timeline with a value.
  - Technical: Row in `keyframes` with `t_ms`, `value_json`, and interpolation.
  - Example: `add-key --track-id <TRACK_UUID> --t 1000 --value '{"x":1}'`.

- Tangent
  - Simple: The direction/steepness of the curve through a key.
  - Technical: Slope (in/out) that shapes Hermite/Bezier interpolation.
  - Example: Stored on keys (engine API) and used during evaluation.

- Autosave / Snapshot
  - Simple: A safety copy of the database.
  - Technical: Copies `project.db` to `snapshots/slotX.db`, ideally after a WAL checkpoint.
  - Example: `scripts/sceneproj.py autosave ...` or Qt shell “Save Snapshot Now”.

---

## Step 2 — Command Bus & Desktop Shell

- Command
  - Simple: One undoable edit (e.g., add a key, move keys).
  - Technical: Implements `do/undo` and optional `diffJson()`; runs inside a DB transaction.
  - Example: `AddKeyframeCommand`, `MoveSelectionCommand`.

- Command Bus / Stack
  - Simple: The system that runs edits and remembers how to undo/redo them.
  - Technical: `CommandStack` executes commands, manages undo/redo vectors, batches, and revision persistence.
  - Example: `desktop/include/verity/command.hpp`, `desktop/src/command.cpp`.

- Batch
  - Simple: Many tiny edits grouped as one (like a drag).
  - Technical: Coalesced set in a single transaction, emitting one revision with multiple item diffs.
  - Example: `beginBatch()/endBatch()` groups commands and writes a single `op=batch` diff.

- Revision (Journal Entry)
  - Simple: A tiny “what changed” note saved in the DB.
  - Technical: Row in `revisions` with label and `diff_json`; used to rebuild state/undo history after restart.
  - Example: Inserted by `CommandStack::execute()` or batch coalescing.

- Transaction
  - Simple: Do all or nothing; never leave half an edit in the DB.
  - Technical: `BEGIN/COMMIT/ROLLBACK` around each command (or whole batch) in SQLite.
  - Example: Storage hooks in `desktop/src/db.cpp`.

- IStorage / SqliteStorage
  - Simple: A plug that connects the command system to a database.
  - Technical: Interface with begin/commit/rollback/addRevision; SQLite implementation when enabled.
  - Example: `desktop/include/verity/db.hpp`, `desktop/src/db.cpp`.

- Qt Shell
  - Simple: The window with dockable panels.
  - Technical: Qt6 Widgets app; hosts Timeline/Graph/Viewport panels and status bar.
  - Example: `desktop/src/main_qt.cpp` with “Save Snapshot Now”.

---

## Step 3 — Curve Engine

- Hermite Curve
  - Simple: Smooth path where you control the slope at each point.
  - Technical: Cubic Hermite interpolation defined by endpoints and tangents (slopes).
  - Example: `createCurve(Hermite)` + `setKeys([...{time,value,inTan,outTan}...])`; tested for linearity.

- Cubic Bezier
  - Simple: Start/end plus two handles (the familiar design‑tool curve).
  - Technical: Cubic polynomial with four control points; we form it via Hermite‑equivalent tangents.
  - Example: `createCurve(BezierCubic)`; test checks midpoint sanity.

- Catmull‑Rom (centripetal)
  - Simple: A curve that goes through all your points.
  - Technical: Interpolating spline using neighboring points; centripetal parameterization reduces looping/overshoot.
  - Example: `createCurve(CatmullRom)`; test checks a 0→1→0 “hill”.

- Segment
  - Simple: The span between two neighbor keys.
  - Technical: Interval `[key[i], key[i+1]]` used for evaluation/LUTs.
  - Example: LUTs are built per segment.

- Arc Length
  - Simple: How far you’ve “traveled” along a curved path.
  - Technical: Integral of speed along the curve; used for constant‑speed reparameterization.
  - Example: Approximated numerically when building LUTs.

- LUT (Lookup Table)
  - Simple: A tiny cheat sheet that maps progress to distance.
  - Technical: Arrays of `u` (0..1) and cumulative arc length `s(u)` sampled per segment.
  - Example: `build_lut()` in `engine/src/engine.cpp`.

- Constant‑Speed Mode
  - Simple: Make motion feel steady even on bends.
  - Technical: Reparameterize `u` using the arc‑length LUT so equal time steps produce near‑equal distance steps.
  - Example: `setConstantSpeed(curveId, true)`; variance check in tests.

- Blending
  - Simple: Mix two curves (like crossfade).
  - Technical: Linear interpolation of sample values: `a*(1‑alpha) + b*alpha`.
  - Example: `evaluateBlended(a,b,0.25,t)` returns ~25% mix.

---

## Step 4 — Viewport & Rendering

- Viewport
  - Simple: The window area where you see the scene.
  - Technical: A render surface embedded in the Qt app (QOpenGLWidget).
  - Example: `ViewportWidget` renders a grid, an animated path, and an FPS overlay.

- QOpenGLWidget
  - Simple: A Qt widget that lets us draw with the GPU.
  - Technical: OpenGL‑backed widget with automatic double buffering and repaint integration.
  - Example: `desktop/src/viewport/ViewportWidget.cpp`.

- FPS Overlay
  - Simple: A small number in the corner showing frames per second.
  - Technical: A moving average of frame times (ms → FPS = 1000/avg) drawn with QPainter.
  - Example: `ViewportWidget::paintGL()` displays `FPS: X.Y`.

- Camera Controls (Pan/Zoom)
  - Simple: Drag to move the view; wheel to zoom in/out.
  - Technical: Update pan offset and zoom factor; map world → screen via scale/translate.
  - Example: `mouseMoveEvent()` and `wheelEvent()` adjust `pan_` and `zoom_`.

- Double Buffering
  - Simple: Draw the next frame off‑screen and swap it in, so it doesn’t flicker.
  - Technical: QOpenGLWidget renders to a back buffer by default; swap occurs after paintGL.
  - Example: Used implicitly by QOpenGLWidget; we call `update()` to schedule the next frame.

- Culling
  - Simple: Skip drawing things that are off‑screen.
  - Technical: Visibility tests reject primitives outside the view frustum.
  - Example: Placeholder for future work when we feed larger path sets to the GPU.

- GPU Buffers (VBOs)
  - Simple: Upload points/lines once and draw them fast.
  - Technical: Vertex Buffer Objects in OpenGL store geometry on the GPU.
  - Example: Viewport draws engine‑sampled trajectories from a VBO as a line strip.

- Keybinds / HUD
  - Simple: Keyboard and on‑screen hints (e.g., press “R” to reset view).
  - Technical: Event handlers update camera state; HUD text and graphs drawn each frame.
  - Example: `ViewportWidget` shows FPS and controls; `R` resets pan/zoom.

## Practical Restore Notes (Revisions)

- Simple: Revisions are tiny change notes. To recover, start from a clean DB and replay the notes. If you replay on a DB that already has the changes, you’ll try to insert duplicates.
- Technical: Replay is idempotent only if we add UPSERT/last‑applied markers. Our E2E restores into a fresh DB to avoid primary‑key collisions while proving journal sufficiency.
- Example: `scripts/e2e_step3.(ps1|sh)` copies `revisions` into a new DB, seeds minimal rows, then runs `--restore`.
