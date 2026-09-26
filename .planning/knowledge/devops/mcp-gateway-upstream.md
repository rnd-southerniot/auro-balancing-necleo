# MCP gateway upstream `auro-balancing-knowledge`
**Category:** devops
**Tags:** mcp, gateway, knowledge-server, 10.10.8.113, fastmcp
**Date:** 2026-09-27
## Context
Serves this repo's knowledge through the SIoT MCP gateway so any session (or another machine)
can pick up the v1 work without the repo checked out. Deploy, update and rollback steps live with
the source in `tools/mcp-server/README.md`; this note records what was done and verified.
## Facts (PROVEN 2026-09-27)
- `127.0.0.1:8022` on `10.10.8.113`, prefix `aurobal`, unit `mcp-auro-balancing-knowledge.service`
  (enabled), server dir `/home/mcp/mcp-servers/auro-balancing-knowledge/`, mirror
  `/home/mcp/knowledge/auro-balancing-necleo/`. Ports 8000–8021 were taken (`ss -tlnp` checked first).
- `uv sync --frozen` on the VM resolved **mcp 1.29.0**. The lock was regenerated locally with
  `uv lock` for this project name (a cloned lock carries the template's name and `--frozen` refuses it).
- Deployed `server.py` sha256 prefix `ae96127ea378395f` = repo copy.
- Proxy config backup: `proxy-config.json.bak-20260927-013654-aurobal`. Verified the diff is exactly
  one appended `servers[]` entry; `gateway`, `auth`, `logging` unchanged. `mcp-proxy.service` restarted.
- Through the gateway (curl from the M5 with the token from `~/.config/siot/mcp-gateway.env`):
  `reload_configuration` → `servers_loaded: 23, enabled_servers: 22`; `check_upstream_health` →
  22/22 healthy including this one; `list_docs` → 29 entries = 29 files staged; `list_skills` → 3;
  `search("STBY pull-down")` → 5 documents; `get_next_session` returns the handout.
## Gateway token vs this Claude session
The Claude Code session's `siot-mcp-gateway` connection was rejected with HTTP 401 "Invalid API key".
The same token read from `~/.config/siot/mcp-gateway.env` **works** with curl (above), and its
SHA-256 is one of the two configured key hashes. So the key on disk is valid; the Claude process
most likely inherited a stale `SIOT_MCP_GATEWAY_TOKEN` from the shell that launched it (ASSUMED —
not verified which value the process holds). Relaunching from a fresh shell should fix it.
## Operating rule
Run `tools/sync-knowledge-mcp.sh` after changing `CLAUDE.md`, `docs/`, `.claude/skills/` or
`.planning/knowledge/`. It aborts if gitleaks flags the staged mirror, which anyone with gateway
access can read.
## Port 8022 was also planned elsewhere
A parallel session (fw-segway-bldc-controller, 2026-09-27) planned `segway-bldc-knowledge` on port
8022 but did not deploy it (its rsync stalled). This upstream now holds 8022. The clash is recorded
in the global profile (`~/.claude/CLAUDE.md` §15); that repo's deploy must pick another port.
