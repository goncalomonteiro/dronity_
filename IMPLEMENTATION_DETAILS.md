# Verity Implementation Details (Steps 0–4)

This file lists only what is implemented today and pragmatic, step‑scoped improvements. It’s organized by step; each step has two sections: Implemented and Improvements.

---

## Step 0 — Project Bootstrap & Guardrails

Implemented
- Monorepo structure: `engine/`, `desktop/`, `backend/`, `web/`, `scripts/`, `ops/`.
- Formatting/linting configs: `/.clang-format`, `/.clang-tidy`, `/.editorconfig`, `CONTRIBUTING.md`.
- CI workflows: `.github/workflows/ci.yml` (build/test/lint) and `.github/workflows/codeql.yml` (CodeQL).

What it is / Why needed
- Monorepo: single repo for all stacks. Why: shared tooling, atomic changes, simpler CI.
- Formatters/linters: consistent code style and early error surfacing. Why: reduce review churn and regressions.
- CI + CodeQL: automated builds/tests and security scanning. Why: prevent breakages and catch issues before merge.

Improvements (Step‑0 scope)
- Add pre‑commit hooks (clang-format, black, ruff) to catch issues before CI.
- Add build caching (ccache/sccache) in CI to speed C++ builds.
- Add CODEOWNERS and Renovate/Dependabot for dependency hygiene.
- Expand CI matrix (Linux/macOS/Windows) for engine/desktop portability checks.

---

## Step 1 — Data Schema & Project Package (Desktop)

Implemented
- `.sceneproj` layout with `project.db` (SQLite/WAL) and snapshot folder.
- Schema and initial migration: projects/scenes/tracks/curves/keyframes/assets/revisions/jobs/events.
- Python CLI: create/open/add‑scene/add‑track/add‑key; autosave/restore with WAL checkpoint.

What it is / Why needed
- `.sceneproj`: a directory package containing the SQLite DB and folders. Why: easy to version, copy, back up.
- SQLite (WAL): embedded relational DB with write‑ahead logging. Why: transactional safety and good read concurrency.
- Schema/migrations: versioned SQL evolution. Why: safe changes over time, reproducible state.
- CLI: scriptable CRUD and snapshots. Why: enables testing and automation before UI exists.

Improvements (Step‑1 scope)
- Add integrity check in CI (`PRAGMA quick_check`) after smoke.
- Add unique/foreign‑key constraints where applicable (e.g., track name uniqueness per scene, optional).
- Add VACUUM INTO‑based snapshot option for safer copies on large DBs.
- Add migration test that applies all migrations to a fresh DB and verifies expected tables/indexes exist.

---

## Step 2 — Command Bus (Undo/Redo) & Desktop Shell

Implemented
- Command framework with transactions, undo/redo, batching, and coalesced revisions.
- SQLite storage (optional) with revision logging and keyframe insert/update/delete helpers.
- Autosave scheduler (C++) and manual snapshot action in Qt shell; desktop runner for headless commands.

What it is / Why needed
- Command Bus: executes edits with do/undo/redo. Why: consistent, reversible editing.
- Transactions: DB `BEGIN/COMMIT/ROLLBACK`. Why: never leave half edits on disk.
- Revisions (journal): minimal diffs stored in DB. Why: recovery after crashes and audit trail.
- Batching: group many micro‑edits into one. Why: correct undo granularity and fewer journal rows.
- Autosave: periodic DB copy with WAL checkpoint. Why: user recovery without data loss.

Improvements (Step‑2 scope)
- Add last‑applied revision pointer to support safe in‑place restore without PK collisions.
- Make inserts idempotent via UPSERT to tolerate partial replays.
- Add batch markers/checksums in `revisions` for robust recovery and diagnostics.
- Extend command tests to cover batch undo/redo and idempotency paths.

---

## Step 3 — Core Curve Engine (C++)

Implemented
- Hermite, Bezier (via Hermite‑equivalent controls), Catmull‑Rom (centripetal) evaluators.
- Constant‑speed remap using per‑segment arc‑length LUTs.
- Blended evaluation between curves; unit tests for correctness; optional microbenchmark.

What it is / Why needed
- Evaluators: math to turn keys into values at time t. Why: power previews, exporters, and validators.
- Constant‑speed: remap parameter so equal time ≈ equal distance. Why: aesthetically even motion; better for later constraints.
- Blending: linear mix of two curves. Why: transitions between paths (fallbacks, edits).
- Tests/bench: correctness and perf signals. Why: protect future refactors, spot regressions.

Improvements (Step‑3 scope)
- Add accuracy tests vs. a reference integrator for arc‑length (mean error and max error thresholds).
- Add vector (x,y,z) curves and shared time base helpers.
- Clamp extreme tangents and add continuity checks (C0/C1) across segments.
- SIMD/vectorization pass for hot paths; parameterize LUT sample counts per segment.
- Provide a thread‑safe evaluate path (const access) and document thread‑safety guarantees.

---

## Step 4 — Desktop Viewport Integration

Implemented
- Qt viewport (QOpenGLWidget) in the shell.
- Engine‑sampled trajectories uploaded to GPU VBO(s); line strip rendering; simple view‑bounds culling.
- Incremental builder (20 Hz) appending batches to VBO to keep UI responsive.
- glMultiDrawArrays path (via QOpenGLExtraFunctions) to reduce draw overhead.
- Actor with its own VAO moving at constant speed along the left‑most path; optional trail; per‑path color toggle; FPS HUD; pan/zoom/reset controls.

What it is / Why needed
- QOpenGLWidget: Qt widget with an OpenGL context and double buffering. Why: smooth integrated rendering inside the Qt app.
- VBO (Vertex Buffer Object): GPU‑resident vertex array. Why: draw many vertices with minimal CPU overhead.
- Line strip: GL primitive connecting a list of points. Why: efficient trajectory drawing.
- View‑bounds culling: skip off‑screen paths via bounding boxes. Why: keep frame time stable with many paths.
- Incremental builder (20 Hz): QTimer assembles a few paths per tick on a worker thread and appends to the VBO on the next frame. Why: avoid long stalls when scenes get heavy.
- Multi‑draw: batch multiple line strips into fewer draw calls. Why: reduce CPU/driver overhead for many paths.
- VAO per stream (paths vs. actor): saved attribute layouts per buffer. Why: prevents state contamination and rendering glitches.
- Actor trail: deque of recent positions rendered as a thin line. Why: makes motion obvious and aids visual debugging.

Notes on multi‑draw availability (Qt)
- Qt may not expose `glMultiDrawArrays` via `QOpenGLExtraFunctions` when:
  - The context is OpenGL ES (function not in ES 2.0 core), or the ANGLE backend is used on Windows.
  - The requested context/profile version is older than where the function is core.
  - Headers or platform plugins compile without that extension path.
- In those cases we fall back to a simple loop of `glDrawArrays` per path (same visuals; slightly higher CPU).

What are the “points in a path”
- The per‑vertex samples `[x0,y0, x1,y1, …]` produced by evaluating curves at regular (or adaptive) times.
- Units: world units (we scale by zoom, add pan in pixels in the shader).
- Density (samples per path) controls smoothness: more points → smoother curves, higher memory; fewer points → jagged lines but cheaper.

Improvements (Step‑4 scope)
- Introduce double‑buffered VBO updates (ping‑pong) to avoid driver stalls during large appends.
- Add instanced rendering for markers/actors and optional index buffers for path reuse.
- Adaptive sampling: increase samples where curvature is high to keep lines smooth with fewer points.
- Camera niceties: inertia, fit‑to‑paths shortcut, screenshot capture.
- Optional headless smoke (Xvfb) that renders a frame and asserts non‑zero draw count (best effort).

Why these improvements matter
- Ping‑pong VBO: isolates GPU reads from CPU writes → fewer micro‑stalls, flatter frame times as scenes grow.
- Instancing + index buffers: draw many identical glyphs/segments in one call and reuse geometry → scales to hundreds of actors/overlays with low CPU.
- Adaptive sampling: preserves visual quality while cutting vertices where curves are flat → better perf and memory use.
- Camera niceties: faster navigation and clear framing → improves authoring productivity and demo polish.
- Headless smoke: basic render regression signal in CI → catches linker/context regressions early without flaky perf checks.

---

Notes
- Feeding real project curves (X/Y/Z) from the DB and wiring edits to the viewport belongs to Step 5 (Authoring v1); today the viewport uses an engine‑generated demo trajectory.
- Automated GUI tests are omitted intentionally; CI validates build/link and non‑GUI paths.
