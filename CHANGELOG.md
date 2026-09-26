# Changelog

All notable changes to this project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] — v1 (JGB37-520 + TB6612FNG)

### Added
- Phase 0: `docs/v1-pinmap.md` — draft v1 pin map, timer and ADC plan, conflict checks against SB62/SB63, the RGB pins, the VCP and TIM10/TIM11, wiring and power-path diagrams. Gate 0 pending the as-wired pin list.
- `.planning/knowledge/` — v1 design decisions, F401RE gotchas, build facts, MCP upstream notes, session logs.
- `CLAUDE.md` execution contract; `docs/v1-plan.md` (approved plan); `docs/NEXT_SESSION.md` handout.
- Project skills in `.claude/skills/` (`.gitignore` now tracks `.claude/skills/` only).
- MCP knowledge upstream `auro-balancing-knowledge`: source in `tools/mcp-server/`, sync via `tools/sync-knowledge-mcp.sh`.
