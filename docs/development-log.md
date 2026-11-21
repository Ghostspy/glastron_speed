# Development record

Plan: `docs/superpowers/plans/2026-11-21-speedometer.md`.

- User approved the matching speed/fuel start and requested GPS speed.
- Work in the requested empty `speed` repository; no prior commits or source tests
  exist. Keep changes here for review. A linked worktree has no base commit yet.
- Execute locally in this session; user has authorized implementation.
- Ruling: use receiver-independent RMC input instead of selecting an unrequested
  GPS module. Module supply and antenna selection remain hardware decisions.
- Preflight: model/parser feed both UI and UART shell through the same timestamped
  setters. Demo and GPS configurations must never mix providers.
