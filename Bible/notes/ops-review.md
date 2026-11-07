# Ops review: Bible/Operations/

Status: executing — phase 1 fixes landed, see commits.

## Goal

Close WANTED item "Review `Bible/Operations/` per AGENTS.md": audit all six
docs plus the evidence folder, fix defects, refresh stale status, split the
workload into small commits. No code changes.

## Audit findings (2026-11-07)

| Doc | Finding | Action |
|---|---|---|
| Agent Guide.md | Title reads `# AGENTS.md — Aur` (wrong file name); content valid. | Retitle to `Agent Guide.md`. |
| Handover.md | Double-encoded mojibake (`â€”`, `Ã—`, `â€`) throughout; stale "DAW P0 safety net in progress"; empty `## A1 Map Editor follow-up`; two overlapping open-work lists; trailing blanks. | Fix encoding, update DAW status to P6.7, delete empty section, merge open-work lists. |
| Roadmap.md | Music Studio bullet predates P4–P6 (no MIDI fidelity, clips, arrangement, launcher). | Refresh the bullet; leave engine items (verified accurate). |
| World Map Topology Handover.md | P3.0 gate complete + P3.1 complete are recorded only at the bottom / in other docs; status buried. | Add a current-status header; keep the historical body (it is the gate evidence record). |
| Project Overview.md | Accurate. | None. |
| MCP Server.md | Accurate. | None. |
| NOTES.md | Append-only log, current. | Append review record. |
| evidence/ | Four PNGs, all referenced by handover text. | Keep. |

## Phases

### Phase 1 — defect fixes (this commit set)

- [x] Plan committed (`<this commit>`).
- [ ] Agent Guide title; Handover mojibake + stale status + empty section +
      merged open-work lists; Roadmap Music Studio bullet.
- [ ] Topology handover status header.
- Acceptance: docs read clean (no mojibake, no stale DAW/P3 status), commit
  per doc group, `git status` clean after push.

### Phase 2 — record

- [ ] NOTES.md review record; WANTED item closed; HANDOVER.md root pointer
      unchanged (still authoritative for active work).

## Rollback

Revert the docs commits; no code is touched.
