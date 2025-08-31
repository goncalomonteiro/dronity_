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
