# Verity Implementation Details (Steps 0–4)

This file lists only what is implemented today and pragmatic, step‑scoped improvements. It’s organized by step; each step has two sections: Implemented and Improvements.

---

## Step 0 — Project Bootstrap & Guardrails

Implemented
- Monorepo structure: `engine/`, `desktop/`, `backend/`, `web/`, `scripts/`, `ops/`.
- Formatting/linting configs: `/.clang-format`, `/.clang-tidy`, `/.editorconfig`, `CONTRIBUTING.md`.
- CI workflows: `.github/workflows/ci.yml` (build/test/lint) and `.github/workflows/codeql.yml` (CodeQL).

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

Improvements (Step‑4 scope)
- Introduce double‑buffered VBO updates (ping‑pong) to avoid driver stalls during large appends.
- Add instanced rendering for markers/actors and optional index buffers for path reuse.
- Adaptive sampling: increase samples where curvature is high to keep lines smooth with fewer points.
- Camera niceties: inertia, fit‑to‑paths shortcut, screenshot capture.
- Optional headless smoke (Xvfb) that renders a frame and asserts non‑zero draw count (best effort).

---

Notes
- Feeding real project curves (X/Y/Z) from the DB and wiring edits to the viewport belongs to Step 5 (Authoring v1); today the viewport uses an engine‑generated demo trajectory.
- Automated GUI tests are omitted intentionally; CI validates build/link and non‑GUI paths.
