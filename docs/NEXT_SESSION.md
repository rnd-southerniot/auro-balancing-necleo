# Next session — handout

Written 2026-09-27 at session close. Read this first, then `CLAUDE.md`.

## Where work stopped

- **Plan approved** 2026-09-25: [docs/v1-plan.md](v1-plan.md) (phases 0–8, gates, port matrix).
- **Phase 0 draft committed:** [docs/v1-pinmap.md](v1-pinmap.md). **Gate 0 is open.**
- Branch `phase/v1-0-pinmap`, pushed to `origin`. No PR opened. `main` is untouched.
- No firmware changed yet. Nothing was flashed. No motion was commanded.
- Repo tooling added this session: `CLAUDE.md`, three project skills, `.planning/knowledge/`,
  the MCP upstream `auro-balancing-knowledge` (gateway `10.10.8.113`, port 8022) and its sync script.

## Blocked on Arif — Gate 0 answers

INA240 is answered (**not fitted**; 5 mΩ, REF1 → VS, REF2 → GND when fitted). Still open:

1. AIN1 / AIN2 / BIN1 / BIN2 / STBY — which Nucleo pins are they wired to? (Proposal: PC10, PC11, PC12, PD2, PC8.)
2. PWMA = PA8 and PWMB = PA9? Encoders still on PA0/PA1 (A) and PA6/PA7 (B)?
3. Vbat divider still 100k/33k on PB1?
4. Nucleo power: 5 V buck into E5V (JP5 on E5V), or VIN?
5. STBY pull-down fitted? One TB6612 module or two?
6. GY-521 still on PB8/PB9? INT pin wired anywhere?
7. RGB LED common anode or common cathode?
8. A UM1724 PDF in `docs/ref/`, so the E5V/USB power-up note can be quoted (st.com was unreachable from this Mac).

## First steps next session

1. `git switch phase/v1-0-pinmap && git pull` and read this file.
2. Fill the **As-wired** column of `docs/v1-pinmap.md` §2 and §9 from Arif's answers; flip
   PROPOSED → PROVEN only for what he confirms. If anything he wired conflicts with SB62/SB63,
   PB3, the RGB pins or TIM10/11, raise it before accepting.
3. Quote the UM1724 E5V note if the PDF is available; otherwise leave §8 UNKNOWN.
4. Commit, run `tools/sync-knowledge-mcp.sh`, ask Arif to confirm **Gate 0**.
5. On Gate 0 PASS: open the Phase 0 PR (or merge per Arif), branch `phase/v1-1-build-ci`, start
   **Phase 1** (`docs/v1-plan.md` §5 Phase 1). Phase 1 needs no hardware.

## Do not

- Start Phase 2 (driver, flashing, motion) before Gate 0 and Phase 1 pass.
- Command motion without Arif confirming wheels-off-ground and the PSU limit (12.0 V, 3 A) in that session.
- Copy from `../auro-balancer`'s working tree — commit `5d9bc9d` only (skill `port-from-auro-balancer`).

## Facts measured this session (PROVEN)

- Default build today = micro-ROS build, links: text 186 560 B, data 3 100 B, bss 73 616 B.
  Compiler actually used = `/opt/arm-gcc` 13.3.1 (the CubeIDE 13.3 path in `cmake/arm-gcc.cmake` is not installed).
- Host-test configure fails: `firmware/tests/` does not exist.
- MCP upstream deployed and answering; see `.planning/knowledge/devops/mcp-gateway-upstream.md`.

## Open items not on the critical path

- WiFi credentials + an internal VM IP are in git history before `3167b03` (already on GitHub).
  Rotation status UNKNOWN from the repo.
- The `siot-mcp-gateway` MCP connection in this Claude session was rejected with 401. The token
  in `~/.config/siot/mcp-gateway.env` is valid: it worked with curl against the gateway on
  2026-09-27 and its hash is configured there. The Claude process most likely inherited a stale
  token from its launching shell (ASSUMED). Relaunch VS Code / Claude Code from a fresh shell.
