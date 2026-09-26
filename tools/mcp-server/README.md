# auro-balancing-knowledge — MCP upstream

The knowledge server deployed on the SIoT MCP gateway for this repo. **This folder is the source
of record**; the gateway runs a copy.

| | |
|---|---|
| Gateway | `10.10.8.113` (`ssh mcp-gateway`, root) |
| Upstream name | `auro-balancing-knowledge`, prefix `aurobal` |
| Listen | `127.0.0.1:8022` (streamable-HTTP `/mcp`) |
| Server dir | `/home/mcp/mcp-servers/auro-balancing-knowledge/` |
| Mirror | `/home/mcp/knowledge/auro-balancing-necleo/` (from `tools/sync-knowledge-mcp.sh`) |
| Unit | `mcp-auro-balancing-knowledge.service` |
| mcp SDK | 1.29.0 (locked; `<2.0` because 2.x removed `mcp.server.fastmcp`) |

Deployed 2026-09-27. The generic doc/search engine is copied verbatim from the AS5047P encoder
upstream (`as5047p-knowledge`); the project tools are specific to this repo.

## Tools

`list_docs` · `get_doc` · `search` — generic, word-anchored search across the mirror.
`get_contract` · `get_next_session` · `get_plan` · `get_spec` · `get_pinmap` · `get_decisions` ·
`get_gotchas` · `get_build_flash` · `list_skills` · `get_skill` · `get_session_log` — named
pointers to the load-bearing documents.

```
call_upstream_tool("auro-balancing-knowledge", "get_next_session", {})
call_upstream_tool("auro-balancing-knowledge", "search", {"query": "STBY pull-down"})
```

## Update content (the common case)

```bash
tools/sync-knowledge-mcp.sh      # stage → gitleaks → rsync → restart → verify handshake
```

## Update the server code

```bash
scp tools/mcp-server/server.py mcp-gateway:/home/mcp/mcp-servers/auro-balancing-knowledge/
ssh mcp-gateway 'chown mcp:mcp /home/mcp/mcp-servers/auro-balancing-knowledge/server.py \
  && systemctl restart mcp-auro-balancing-knowledge.service \
  && systemctl is-active mcp-auro-balancing-knowledge.service'
```

A **new tool** also needs the gateway to re-list it: `reload_configuration` via the gateway, or
`systemctl restart mcp-proxy.service` (that restart briefly interrupts every upstream).

## Rollback

```bash
ssh mcp-gateway 'systemctl disable --now mcp-auro-balancing-knowledge.service'
# then restore the proxy config from its pre-change backup (name recorded in
# .planning/knowledge/devops/mcp-gateway-upstream.md):
ssh mcp-gateway 'cd /home/mcp/mcp-gateway/configs && \
  cp -p proxy-config.json.bak-<stamp>-aurobal proxy-config.json && systemctl restart mcp-proxy.service'
```
