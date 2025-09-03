# Verity Implementation Details (Steps 0–4)

This document is organized by step. Each step lists “implementation topics” (what is built today) and “future improvements” (not yet implemented). Every topic follows the same structure: name, description, technical explanation, “before” behavior, and why it’s needed.

---

## Step 0 — Project Bootstrap & Guardrails

Implemented (grouped)

A. Reliability & Tooling
1) Monorepo layout
1.1 What: One repository with `engine/`, `desktop/`, `backend/`, `web/`, `scripts/`, `ops/`.
1.2 How it works: Shared root tooling; each sub‑project has its own build but shares CI config.
1.3 Previously: Scattered repos complicate atomic changes and CI orchestration; version drift between stacks.
1.4 Why it matters: Atomic commits across stacks; simpler onboarding; consistent tools.

2) CI & CodeQL
2.1 What: `.github/workflows/ci.yml` for builds/tests; `.github/workflows/codeql.yml` for security scans.
2.2 How it works: Matrix builds, language‑specific jobs; CodeQL database + queries.
2.3 Previously: Manual testing; regressions caught late; no security signal.
2.4 Why it matters: Early failure signals; secure defaults; repeatable quality gates.

B. Quality of Life
3) Formatting/linting (C++/Python/TS)
3.1 What: `/.clang-format`, `/.clang-tidy`, `.editorconfig`, Black/Ruff, ESLint/Prettier.
3.2 How it works: Enforce style; static checks in CI and locally.
3.3 Previously: Inconsistent style; review churn; subtle bugs slip in.
3.4 Why it matters: Faster reviews; fewer regressions; uniform code quality.

Future improvements (not implemented)

A. Reliability & Tooling
4) Build cache in CI
4.1 What: ccache/sccache for C++.
4.2 How it works: Hash compiler invocations; reuse artifacts.
4.3 Previously: Full rebuilds slow CI.
4.4 Why it matters: Faster signal, lower costs.

B. Quality of Life
5) Pre‑commit hooks
5.1 What: Local git hooks running format/lint.
5.2 How it works: `pre-commit` framework; staged files only.
5.3 Previously: CI catches trivial issues; devs wait for cycles.
5.4 Why it matters: Faster feedback; less CI churn.

---

## Step 1 — Data Schema & Project Package (Desktop)

Implemented (grouped)

A. Reliability & Data
1) `.sceneproj` package
1.1 What: Directory with `project.db` and `/assets`, `/bakes`, `/thumbs`, `/snapshots`.
1.2 How it works: SQLite DB with WAL; filesystem layout is stable and script‑friendly.
1.3 Previously: Ad‑hoc files; fragile copy/backup; no clear on‑disk contract.
1.4 Why it matters: Durable, portable project format compatible with tooling and VCS.

2) SQLite schema + migrations
1.1 What: Tables for projects/scenes/tracks/curves/keyframes/assets/revisions/jobs/events.
1.2 How it works: `schema.sql` and versioned `migrations/V####__*.sql`.
1.3 Previously: Schema drift; manual changes risky; no upgrade path.
1.4 Why it matters: Safe schema evolution; reproducible DB state.

3) Project CLI
1.1 What: `scripts/sceneproj.py` to create/open/add scene/track/key and autosave/restore.
1.2 How it works: Applies migrations; WAL checkpoint before snapshot.
1.3 Previously: Hard to test flows; manual DB poking; error‑prone.
1.4 Why it matters: Early automation and smoke testing before UI exists.

Future improvements (not implemented)

A. Reliability & Data
4) Integrity checks
4.1 What: `PRAGMA quick_check` in CI.
4.2 How it works: Run after smoke; fail on corruption.
4.3 Previously: Latent issues only surface later.
4.4 Why it matters: Early detection of DB problems.

5) Safer snapshots
5.1 What: `VACUUM INTO` or backup API for large DBs.
5.2 How it works: Copy without locking main DB for long.
5.3 Previously: File copy during WAL write windows can stall.
5.4 Why it matters: Robust autosave under heavy edits.

---

## Step 2 — Command Bus (Undo/Redo) & Desktop Shell

Implemented (grouped)

A. Reliability
1) Command Bus with transactions and batching
1.1 What: do/undo/redo with grouped edits.
1.2 How it works: Each command executes inside `BEGIN/COMMIT` (batch uses a single transaction); coalesced revision per batch.
1.3 Previously: Inconsistent edits; half‑applied changes on crash; many tiny undos.
1.4 Why it matters: Reliable, user‑friendly editing with correct undo granularity.

2) SQLite storage with revisions
1.1 What: IStorage/SqliteStorage writing revisions + keyframe helpers.
1.2 How it works: WAL mode; `revisions(label, diff_json, created_at)`.
 1.3 Previously: No recovery trail; hard to audit changes.
1.4 Why it matters: Crash recovery and change history.

3) Autosave
1.1 What: Periodic snapshot of `project.db`.
1.2 How it works: `wal_checkpoint(FULL)` then copy to `snapshots/slot1.db`.
1.3 Previously: Crash can lose minutes of work.
1.4 Why it matters: Safe restore point without user intervention.

4) Qt shell scaffold
1.1 What: Dockable Timeline/Graph/Viewport window.
1.2 How it works: QMainWindow + QDockWidget; basic menu/status.
1.3 Previously: No host for tools; fragmented UX.
1.4 Why it matters: Foundation for authoring UI.

Future improvements (not implemented)

A. Reliability
5) Last‑applied revision pointer
5.1 What: Track max applied revision id.
5.2 How it works: Apply only newer revisions on restore; avoid PK conflicts.
5.3 Previously: Replaying on same DB can collide with existing rows.
5.4 Why it matters: Safe in‑place restore.

6) UPSERT/idempotency
6.1 What: INSERT … ON CONFLICT DO NOTHING for journal replays.
6.2 How it works: Tolerates partial application without errors.
6.3 Previously: Replaying a duplicate insert raises.
6.4 Why it matters: Robust recovery.

---

## Step 3 — Core Curve Engine (C++)

Implemented (grouped)

B. Accuracy
1) Curve evaluators (Hermite/Bezier/Catmull‑Rom)
1.1 What: Functions to compute values at time t.
1.2 How it works: Segment search; basis evaluation (Hermite tangents; Bezier via Hermite‑equivalent; Catmull‑Rom centripetal).
1.3 Previously: No way to turn keys into motion; UI can’t preview.
1.4 Why it matters: Deterministic sampling for previews/exports/validators.

2) Constant‑speed remap
1.1 What: Even motion along curves.
1.2 How it works: Per‑segment arc‑length LUT; invert s(u) to get u for equal distances.
1.3 Previously: Drone appears to speed up/slow down in bends.
1.4 Why it matters: Aesthetically pleasing, predictable motion.

3) Blending
1.1 What: Mix two curves.
1.2 How it works: `a*(1−alpha)+b*alpha` at the same time t.
1.3 Previously: Abrupt jumps between paths.
1.4 Why it matters: Smooth transitions (e.g., fallback→show path).

Future improvements (not implemented)

B. Accuracy
4) Accuracy tests vs integrator
4.1 What: Compare LUT remap against high‑precision numeric integration.
4.2 How it works: Error thresholds (mean/max) per segment.
4.3 Previously: LUT quality unbounded; tweaks risky.
4.4 Why it matters: Quality guardrails.

5) Adaptive LUT/sample counts
5.1 What: More samples where curvature is high.
5.2 How it works: Curvature/angle error‑driven refinement.
5.3 Previously: Uniform sampling over/under‑spends vertices.
5.4 Why it matters: Smooth curves at lower cost.

---

## Step 4 — Desktop Viewport Integration

Implemented (grouped)

A. Quality of Life
  1) QOpenGLWidget viewport
    1.1 What: Qt widget with an OpenGL context and double buffering.
    1.2 How it works: Render to back buffer; swap to front; integrated with Qt event loop.
    1.3 Previously: Rendering directly to the front buffer can expose half‑drawn frames and tearing because the user sees the framebuffer mid‑update.
    1.4 Why it matters: Smooth, flicker‑free rendering embedded in the app window.

B. Performance
  2) VBO + VAO for paths
    1.1 What: Store trajectory points in GPU memory; VAO records attribute layout.
    1.2 How it works: One GL_ARRAY_BUFFER with [x,y] pairs; VAO binds location 0 to 2 floats per vertex.
    1.3 Previously: CPU would resend geometry every frame; high driver overhead.
    1.4 Why it matters: Low‑CPU, high‑throughput drawing.

  3) Line‑strip rendering
    1.1 What: Draw N points as a connected polyline.
    1.2 How it works: Single GL_LINE_STRIP draw → N−1 segments; no index buffer required.
    1.3 Previously: Many per‑segment draws or per‑line overhead.
    1.4 Why it matters: Efficient trajectory visualization.

  4) View‑bounds culling
    1.1 What: Skip paths outside the camera view.
    1.2 How it works: Precompute AABB per path (min/max in world); intersect with view rectangle; skip if disjoint.
    1.3 Previously: Waste time drawing off‑screen paths; frame time grows with scene size.
    1.4 Why it matters: Stable FPS regardless of off‑screen content.

  5) Incremental builder (20 Hz)
    1.1 What: Build a few paths per timer tick on a worker thread.
    1.2 How it works: QTimer fires; thread samples engine curves → produces vertices; UI thread appends to VBO next frame.
    1.3 Previously: One big build stalls the UI while sampling and uploading.
    1.4 Why it matters: Responsive UI while heavy scenes stream in.

  6) Multi‑draw path (with fallback)
    1.1 What: Draw many paths in one call when available.
    1.2 How it works: Resolve `glMultiDrawArrays`; if available and not forced off, batch line strips; else loop `glDrawArrays`.
    1.3 Previously: One draw per path, high CPU/driver overhead.
    1.4 Why it matters: Fewer draw calls → lower CPU, better scalability.

  7) Separate VAOs (paths vs actor) + actor trail
    1.1 What: Two VAOs prevent state collisions; optional trail visualizes motion.
    1.2 How it works: Path VAO binds path VBO; actor VAO binds small dynamic VBO; trail uses QPainter overlay.
    1.3 Previously: Shared VAO caused attribute state corruption → glitches; hard to see motion.
    1.4 Why it matters: Robust rendering and clearer motion cues.

Future improvements (not implemented)

B. Performance
  8) Ping‑pong VBO updates
    8.1 What: Alternate two VBOs for updates/draws.
    8.2 How it works: CPU writes to one while GPU reads the other; swap next frame.
    8.3 Previously: Appending to an in‑use VBO may stall the driver (sync).
    8.4 Why it matters: Flatter frame time during heavy updates.

  9) Instanced markers and index buffers
    9.1 What: Draw many identical glyphs with per‑instance data; dedupe vertices via indices.
    9.2 How it works: One mesh in a VBO; per‑instance transforms/colors; optional IBO for reuse.
    9.3 Previously: Many small meshes and duplicate geometry cost CPU and memory.
    9.4 Why it matters: Scale actors/overlays to hundreds with low overhead.

  10) Adaptive sampling
    10.1 What: Densify points where curvature is high; thin where flat.
    10.2 How it works: Curvature/angle or screen‑space error threshold; refine until within tolerance.
    10.3 Previously: Uniform sampling over/under‑spends vertices.
    10.4 Why it matters: Preserve visual quality while cutting vertices.

A. Quality of Life
  11) Camera UX (inertia, fit‑to‑paths, screenshots)
    11.1 What: Smoother motion, quick framing, easy sharing.
    11.2 How it works: Exponential smoothing on pan/zoom; compute unified AABB → set pan/zoom; FBO readback for PNG.
    11.3 Previously: Jumpier navigation; manual framing; cumbersome sharing.
    11.4 Why it matters: Faster authoring and clearer demos.
