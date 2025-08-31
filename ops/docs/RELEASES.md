# Release & Versioning (Bootstrap)

Channels
- `alpha`: fast iteration; unstable
- `beta`: feature-complete; stabilization
- `stable`: production-ready

Tags
- Semantic tags per stack (subject to refinement):
  - Engine/Desktop: `engine-vX.Y.Z`, `desktop-vX.Y.Z`
  - Backend: `backend-vX.Y.Z`
  - Web: `web-vX.Y.Z`

Notes
- Conventional Commits required to enable automated release tooling later (e.g., semantic-release/changesets).
- CI currently builds/tests; release packaging will be added in later steps.

