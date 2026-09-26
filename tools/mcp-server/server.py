#!/usr/bin/env python3
"""
auro-balancing-knowledge MCP server.

Serves the rnd-southerniot/auro-balancing-necleo repo's knowledge (CLAUDE.md,
docs/ incl. the v1 plan, pin map and next-session handout, project skills,
.planning/knowledge, auto-memory) as queryable MCP tools, so an agent working
through the SIoT MCP gateway can pick up the NUCLEO-F401RE self-balancing robot
(v1: JGB37-520 + TB6612FNG) without local file access.

Search engine (list_docs/get_doc/search) cloned verbatim from the gateway's
AS5047P encoder upstream (tools/mcp-server/server.py of that repo, 2026-09-22);
the project tools below are specific to this repo.

Upstream contract (matches the gateway's other servers): a FastMCP server
serving /mcp over streamable-HTTP.
  Run: uv run python server.py streamable-http 127.0.0.1 8022

Knowledge root is an rsync'd mirror of the repo on the VM, refreshed by
tools/sync-knowledge-mcp.sh in the repo. Override with AUROBAL_KNOWLEDGE_ROOT.
"""
import json
import os
import re
import sys
from pathlib import Path

from mcp.server.fastmcp import FastMCP

transport_mode = sys.argv[1] if len(sys.argv) > 1 else "streamable-http"
server_host = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"
server_port = int(sys.argv[3]) if len(sys.argv) > 3 else 8022

ROOT = Path(
    os.environ.get("AUROBAL_KNOWLEDGE_ROOT", "/home/mcp/knowledge/auro-balancing-necleo")
).resolve()

DOC_DIRS = ["docs", "skills", "memory"]
EXTRA_FILES = ["CLAUDE.md", "README.md"]

mcp = FastMCP("auro-balancing-knowledge", host=server_host, port=server_port)


def _iter_docs():
    for d in DOC_DIRS:
        base = ROOT / d
        if base.is_dir():
            for p in sorted(base.rglob("*.md")):
                yield p
    for f in EXTRA_FILES:
        p = ROOT / f
        if p.is_file():
            yield p


def _rel(p: Path) -> str:
    try:
        return str(p.relative_to(ROOT))
    except ValueError:
        return str(p)


def _title(p: Path) -> str:
    try:
        for line in p.read_text(errors="replace").splitlines():
            if line.startswith("# "):
                return line[2:].strip()
    except Exception:
        pass
    return ""


@mcp.tool()
def list_docs() -> list[dict]:
    """List available auro-balancing-necleo knowledge documents (path + title)."""
    return [{"path": _rel(p), "title": _title(p)} for p in _iter_docs()]


@mcp.tool()
def get_doc(path: str) -> str:
    """Return the full markdown of a knowledge doc by its path (from list_docs())."""
    p = (ROOT / path).resolve()
    if p != ROOT and ROOT not in p.parents:
        return "error: path outside knowledge root"
    if not p.is_file():
        return f"error: not found: {path}"
    return p.read_text(errors="replace")


# Dropped from a query while other terms remain. A knowledge base is asked questions in prose —
# "why does it drop rather than queue" — and requiring "does"/"rather"/"than" to appear turns a
# perfectly reasonable question into zero results. Negations ("no", "not", "never") are deliberately
# NOT here: in these documents they carry the meaning.
_STOPWORDS = {
    "a", "an", "and", "are", "as", "at", "be", "but", "by", "can", "do", "does", "for", "from",
    "how", "i", "if", "in", "into", "is", "it", "its", "of", "on", "or", "our", "so", "than",
    "that", "the", "their", "them", "then", "there", "these", "they", "this", "to", "was", "we",
    "what", "when", "where", "which", "why", "will", "with", "you",
}


def _term_re(term: str) -> "re.Pattern[str]":
    """
    A term matches at the **start of a word**, not anywhere inside one.

    Plain substring matching makes short terms catastrophically noisy: searching for `no` also hits
    *know*, *cannot* and *nothing*, which is how a four-word query reported 1527 matches across the
    knowledge base. Anchoring to a word start keeps the useful looseness — `queue` still finds
    *queues* and *queueing* — while `no` stops matching the middle of unrelated words.
    """
    return re.compile(r"(?<![a-z0-9])" + re.escape(term))


def _flatten(text: str) -> str:
    """
    Whitespace-normalised text.

    This is what lets a phrase match across a **hard line wrap**. Every document here is prose wrapped
    at ~100 characters, so line-at-a-time matching misses any phrase unlucky enough to straddle a
    break — which is most of them.
    """
    return " ".join(text.lower().split())


@mcp.tool()
def search(query: str, max_results: int = 8, per_doc: int = 2) -> dict:
    """
    Ranked search across all knowledge docs.

    Returns {query, total, returned, truncated, results:[{path,line,score,snippet}]}.

    Three deliberate behaviours, each replacing something the previous implementation got wrong:

    1. **Terms, not one literal string.** It used to test the entire query as a single substring, so
       any multi-word question missed unless quoted verbatim from a document.
    2. **Phrases match across line wraps**, because these files are hard-wrapped prose.
    3. **Everything is scanned, ranked, and only then truncated** — and at most [per_doc] hits come
       from any one document. The old version returned at the eighth match in directory order, so a
       common term returned whichever file happened to be scanned first: searching "mqtt" gave eight
       hits from one file and never reached the reference document for it.

    [total] and [truncated] exist because silent truncation reads as "that is all there is".
    """
    raw = query.strip()
    if not raw:
        # Same keys as every other return: a caller that indexes the reply must not have to special
        # case the empty query.
        return {
            "query": query,
            "documents": 0,
            "total": 0,
            "returned": 0,
            "truncated": False,
            "results": [],
        }

    phrase = _flatten(raw)
    terms = [t for t in phrase.split(" ") if t]
    # Keep the stopwords if that is all there is, so searching "how to" still does something.
    meaningful = [t for t in terms if t not in _STOPWORDS] or terms
    patterns = [_term_re(t) for t in meaningful]

    scored: list[tuple] = []

    for p in _iter_docs():
        try:
            text = p.read_text(errors="replace")
        except Exception:
            continue
        lines = text.splitlines()
        flat = _flatten(text)
        rel = _rel(p)
        rel_l = rel.lower()

        matched = [t for t, pat in zip(meaningful, patterns) if pat.search(flat) or pat.search(rel_l)]
        if not matched:
            continue
        complete = len(matched) == len(meaningful)
        has_phrase = len(terms) > 1 and phrase in flat

        # Best lines within the document: the ones carrying the most terms, headings first.
        line_hits: list[tuple[int, int]] = []
        for i, line in enumerate(lines):
            low = line.lower()
            carried = sum(1 for pat in patterns if pat.search(low))
            if carried:
                heading = 1 if line.lstrip().startswith("#") else 0
                line_hits.append((carried + heading, i))
        if not line_hits:
            # The terms are in the document but split across wrapped lines. Still a real match, so
            # surface the document rather than discarding it for a formatting accident.
            line_hits = [(0, 0)]
        line_hits.sort(key=lambda h: (-h[0], h[1]))

        score = (
            (100 if has_phrase else 0)
            + 40 * len(matched)
            + (20 if complete else 0)
            + (10 if any(pat.search(rel_l) for pat in patterns) else 0)  # the path is a strong hint
            + min(line_hits[0][0], 5)
        )
        scored.append((score, rel, lines, line_hits, complete))

    # Prefer documents carrying every term; fall back to partial matches only when none does, so a
    # precise query is not diluted by documents that merely share a word with it.
    if any(entry[4] for entry in scored):
        scored = [entry for entry in scored if entry[4]]

    # Counted **after** the filter, over exactly the documents [documents] refers to. Counting before
    # it meant the two numbers described different sets — 4 documents beside 1296 matches — which is
    # the same defect as a truncation flag that disagrees with its own totals.
    total = sum(len(entry[3]) for entry in scored)

    scored.sort(key=lambda entry: (-entry[0], entry[1]))

    results: list[dict] = []
    for score, rel, lines, line_hits, _complete in scored:
        for _rank, i in line_hits[:per_doc]:
            results.append(
                {
                    "path": rel,
                    "line": i + 1,
                    "score": score,
                    "snippet": "\n".join(lines[max(0, i - 1): i + 2]),
                }
            )

    returned = results[:max_results]
    return {
        "query": raw,
        "documents": len(scored),
        "total": total,
        "returned": len(returned),
        # Compared against [total], not against the already-capped list: hits dropped by [per_doc]
        # are still hits the caller is not seeing. Reporting "truncated: false" beside "total: 10,
        # returned: 3" would be a contradiction, and the whole reason this field exists is that a
        # silently shortened result set reads as "that is all there is".
        "truncated": total > len(returned),
        "results": returned,
    }



# --- project-specific convenience tools ---------------------------------------------------------
# Thin, named pointers into the mirror so an agent reaches the load-bearing documents without
# knowing the layout. Mirror layout (see tools/sync-knowledge-mcp.sh):
#   CLAUDE.md, README.md          repo root
#   docs/*.md                     v1 plan, pin map, next-session handout, spec prompt
#   docs/legacy/*.md              pre-v1 root docs (DBH-12V + MG513P30 era; several are stale)
#   skills/<name>/SKILL.md        project skills
#   memory/knowledge/...          .planning/knowledge (architecture, gotchas, devops, sessions)
#   memory/*.md                   auto-memory quick facts


@mcp.tool()
def get_contract() -> str:
    """The repo CLAUDE.md: locked decisions, hardware safety gates, verified build facts, phase
    table, known defects and the State block. START HERE for current status."""
    return get_doc("CLAUDE.md")


@mcp.tool()
def get_next_session() -> str:
    """The next-session handout: where work stopped, what is blocked on whom, and the exact first
    steps for the next session."""
    return get_doc("docs/NEXT_SESSION.md")


@mcp.tool()
def get_plan() -> str:
    """The approved v1 plan (2026-09-25): decisions D-A..D-D and E-1..E-5, findings, Phase 0 pin
    plan, Phases 1-8 with files, port sources and PASS/FAIL gates, risks, verification."""
    return get_doc("docs/v1-plan.md")


@mcp.tool()
def get_spec() -> str:
    """Arif's original v1 spec prompt: target hardware (JGB37-520, TB6612FNG, INA240, 3S), the
    B1-B12 measurements, controller architecture, supervisor FSM and acceptance criteria.
    Where it disagrees with get_plan(), the plan wins (e.g. RGB pins, COBS, single-motor claim)."""
    return get_doc("docs/v1-spec-prompt.md")


@mcp.tool()
def get_pinmap() -> str:
    """Phase 0 pin map for NUCLEO-F401RE + TB6612FNG: pin table with PROVEN/PROPOSED/PENDING labels,
    SB62/SB63 and PB3 constraints, timers, ADC, wiring and power-path diagrams, Gate 0 questions.
    DRAFT until Arif confirms the as-wired pins."""
    return get_doc("docs/v1-pinmap.md")


@mcp.tool()
def get_decisions() -> str:
    """Locked v1 design decisions with the reason for each (base repo, serial-only build, center-
    aligned PWM, IT-driven IMU, no inner RPM PID, framing, INA240 not fitted, interim protection)."""
    return get_doc("memory/knowledge/architecture/v1-design-decisions.md")


@mcp.tool()
def get_gotchas() -> str:
    """NUCLEO-F401RE traps found in this code: SB62/SB63 must stay open, PB3 is SWO, the IWDG reset
    loop root cause, HAL_I2C_Mem_Read_DMA still blocks, 1320 CPR quantisation, IMU axis pairing."""
    return get_doc("memory/knowledge/gotchas/f401re-v1-gotchas.md")


@mcp.tool()
def get_build_flash() -> str:
    """Measured build/test/flash facts: which compiler is actually used, micro-ROS auto-enable,
    firmware size, benign link warnings, the missing host-test directory, flash preconditions."""
    return get_doc("memory/knowledge/devops/build-flash.md")


@mcp.tool()
def list_skills() -> list[dict]:
    """List the project skills shipped with this repo."""
    base = ROOT / "skills"
    if not base.is_dir():
        return []
    return [
        {"skill": p.parent.name, "path": _rel(p), "title": _title(p)}
        for p in sorted(base.glob("*/SKILL.md"))
    ]


@mcp.tool()
def get_skill(skill: str) -> str:
    """Return a project skill's SKILL.md by name (from list_skills()), e.g.
    'nucleo-f401-build-flash', 'v1-bench-measurement' or 'port-from-auro-balancer'."""
    return get_doc(f"skills/{skill}/SKILL.md")


@mcp.tool()
def get_session_log() -> str:
    """Most recent dated session log: what was done, what was proven, and what is next."""
    base = ROOT / "memory" / "knowledge" / "sessions"
    if not base.is_dir():
        return "error: no session logs"
    logs = sorted(base.glob("*.md"))
    if not logs:
        return "error: no session logs"
    return logs[-1].read_text(errors="replace")


if __name__ == "__main__":
    print(
        f"auro-balancing-knowledge MCP on {server_host}:{server_port} "
        f"({transport_mode}); root={ROOT}",
        flush=True,
    )
    mcp.run(transport="stdio" if transport_mode == "stdio" else "streamable-http")
