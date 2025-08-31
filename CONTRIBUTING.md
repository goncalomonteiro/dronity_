Contributing Guide

Branching & Commits
- Use Conventional Commits: feat:, fix:, docs:, chore:, refactor:, test:, build:, ci:
- Keep PRs focused; include a concise description and context.

Code Style
- C++: clang-format (config at repo root), clang-tidy in CI
- Python: black + ruff; type hints encouraged
- Web/TS: eslint + prettier; TypeScript strict mode

Local Validation
- Engine/Desktop: CMake build must pass; add unit tests where practical
- Backend: `ruff check . && black --check . && pytest`
- Web: `npm run lint && npm run typecheck && npm test && npm run build`

Releases & Versioning
- Semantic version tags per stack will be introduced; for now, follow Conventional Commits to enable automated release tooling later.

ADRs
- Propose architecture decisions under `ops/docs/adr/` using the template.

