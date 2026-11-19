# Aur documentation roles and overlap reconciliation
Status: P0 audit complete; next: P1 reconcile the current project-doc spine and mark superseded planning material without discarding source history.

## Goal
Give every current document one distinct authority and make a fresh session's reading order unambiguous. Keep verified state, owner asks, stage roadmap, gated implementation inventory, active initiative plans, historical records, and design sources separate. Preserve owner intent and unrelated worktree changes.

## Contracts (binding)

- Root `AGENTS.md` is operating policy; root `HANDOVER.md` is the concise current pointer and exact next action; root `WANTED.md` is the ordered owner queue; root `.md` is verified product state, decisions, and open defects; root `ROADMAP.md` is strategic stages; root `IMPLEMENTATION.md` is the open, evidence-backed item/gate ledger.
- `Bible/README.md` is the documentation index and source-precedence guide. Focused design bibles carry confirmed design direction. `Bible/notes/` carries active scoped plans/briefs; `Bible/Operations/NOTES.md` is dated implementation history. The old `Bible/Operations/{Handover,Roadmap,Project Overview}.md` remain historical snapshots, not competing live status docs.
- Every owner ask and active plan must have one canonical path, a single status, and consistent pointers. Historical context may repeat facts only when clearly dated and explicitly non-authoritative.
- Do not discard original owner-provided plans or overwrite unrelated dirty paths. Superseded root copies become short pointers; duplicated draft plans stay available as clearly marked historical/proposal material unless a distinct owner-approved action authorizes removal.
- Documentation-only steps do not alter source or test behavior. Preserve the graphics policy and all owner-gated decisions.

## Current shape (verified)

- Root status spine exists, but `ROADMAP.md` still describes already-landed Forge, map-generator and bandit work as queued; `PROJECT.md` says no open defects while `IMPLEMENTATION.md` retains stale W-03 language; `IMPLEMENTATION.md` lists W-01 as open despite its only explicitly open beach-asset phase being owner-gated.
- `HANDOVER.md` and `WANTED.md` point to active plans, but the reading sequence, exact highest-priority unblocked action, and the status of a new two-copy master HD plan are not reconciled. Root HANDOVER describes the third globe LOD as next while the broad master plan also mixes already-landed GPU/globe work and gated WorldSim work into the same unchecked section.
- `aur_master_expansion_hd_upgrade_plan.md` at the project root and `Bible/notes/aur_master_expansion_hd_upgrade_plan.md` are competing versions of a broad plan; neither is listed as an active plan. The Bible copy has already been substantially rewritten in the worktree. Preserve both bodies/history and identify one as a non-authoritative proposal, not as a second execution queue.
- `Bible/notes/globe-view.md` is the focused W-04 plan; `globe-expand.md`, platformer, World Map work packages, WorldSim, DAW and other notes each own separate scopes. `Bible/Operations/Handover.md`, `Roadmap.md`, and `Project Overview.md` are already labelled historical but contain obsolete claims.
- Protected pre-existing worktree state at audit: modified `AIO-Win.bat`, modified `ROADMAP.md`, untracked `cmake/`, and two untracked master-plan copies. Do not stage or revert these paths as this reconciliation's work.

## Hazards found
- A documentation rewrite can silently reorder owner asks, close owner-gated decisions, or misrepresent current source/test evidence. Verify every closure against current code and test evidence; leave unresolved owner choices open.
- Several files deliberately preserve old material for provenance. Correct their authority labels and links rather than destructively deleting source history.
- The repo shares a working tree. Make atomic commits containing only this plan or specifically reconciled document paths, and leave the identified unrelated changes untouched.
- A full project-wide Markdown rewrite is unnecessary and risks losing lore/source authority. Scope to live status spine, active-plan index, duplicate master-plan identity, and historical Operations index/pointers.

## Target layout
- **Operating:** `AGENTS.md`.
- **Live handoff / owner queue / verified state / strategic order / gate ledger:** `HANDOVER.md`, `WANTED.md`, `PROJECT.md`, `ROADMAP.md`, `IMPLEMENTATION.md`, respectively.
- **Design source and index:** `Bible/README.md`, focused design bibles, immutable `Bible/Archive/`.
- **Execution truth:** one scoped plan per initiative under `Bible/notes/`; the owner queue links the active ones and the handover identifies the one resumed next.
- **History:** dated `Bible/Operations/NOTES.md`; historical Operations snapshots labelled and linked only.
- Broad master expansion plan retained as an archived proposal/pointer, not an independently executable plan or active queue.

## Phases
### P0 Safety net — audit
- [x] Inventory Markdown documents, live status spine, plan links, duplicated master-plan copies, relevant operation snapshots, and dirty paths.
- [x] Read root operating guidance, the current handover and owner queue, every in-repository active plan, the project state, and the two estate plans referenced by the queue.
- [x] Record conflicting/stale claims and the evidence that can resolve them without making owner-only decisions.

Acceptance: this audit records concrete overlaps, the canonical roles, protected paths, and the next step. Plan committed alone before reconciliation.

### P1 Canonical status spine
- [ ] Reconcile the owner queue, status pointer, project facts, strategic roadmap, and implementation gates against current plans/source evidence. Keep the WANTED order and owner gates; choose the highest-priority actionable item rather than leapfrogging the queue.
- [ ] Give every active plan exactly one canonical WANTED entry, record parked/owner-gated plans as such, and make HANDOVER's active-plan index agree. Mark the two master-plan copies as a single non-authoritative proposal with a canonical pointer; do not lose owner-provided material.
- [ ] Mark completed roadmap items as landed and remove stale duplicate/open claims from the gate ledger only where current evidence supports closure.

Acceptance: each live ask and active plan has a single status and consistent links across the five root status documents; no owner-gated choice is invented.
End tag: `doc-reconciliation-p1`.

### P2 Bible navigation and closeout
- [ ] Make `Bible/README.md` classify active plans, historical Operations snapshots, dated notes, and proposal/archive material with pointers to their canonical owners.
- [ ] Update `Bible/Operations/NOTES.md` with the reconciliation decisions, verified evidence, and remaining defects; trim no historical evidence except clearly redundant live-status repetitions.
- [ ] Validate relative links and cross-document item status; update HANDOVER's exact next action and plan pointers; remove this closed plan from the active index and delete it after acceptance (git retains its history).

Acceptance: a new agent can determine authority and read order from the roots and Bible index without treating history/proposals as current status; links and queue agree; unrelated dirty paths remain untouched.
End tag: `doc-reconciliation-p2`.

## Acceptance
- Every live status document and active initiative plan has one clearly named responsibility and no contradictory status claims.
- Owner queue order, owner-gated choices, and the graphics backend policy are preserved.
- Historical records and original proposal content are retained but cannot be mistaken for the live plan.
- Focused conformance/document checks pass or failures are recorded precisely; no implementation changes or unrelated paths enter the commits.

## Rollback
Revert only the named reconciliation commit(s), never the protected worktree changes. If evidence contradicts a planned closure, correct the plan before proceeding and leave the item open.

## Decisions and deviations
- The root `ROADMAP.md` and `IMPLEMENTATION.md` have exclusive live authority for staged work and gated item status; the similarly named Operations files are history only.
- The two master HD plan copies are retained as historical/proposal text rather than merged wholesale into the live roadmap: they overlap owner asks, contain already-landed tasks, and mix owner-gated work with executable phases. Promote an individual item only through its canonical focused plan and the ordered WANTED queue.
- Reconciliation is documentation-only. Existing owner decisions (beach tile selection, platformer floor selection, WorldSim temperature/persistence choices, parked DAW P7a) remain gates, not inferred approvals.
