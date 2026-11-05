# AGENTS.md — Aur
> First boot: read `HANDOVER.md` and `WANTED.md`; keep both lean - drop verified items (git is the history).

> Repository guidance is maintained in [Bible/Operations/Agent Guide.md](Bible/Operations/Agent%20Guide.md). Read it before making changes.


## Self-repair (Bible)

Before moving on from any task, update the relevant Bible/<module>/NOTES.md with findings, decisions, and defects discovered. Keep AGENTS.md at the project root as the single index that points into Bible/. Clean up any temporary logs or scratch files created during the work so they do not accumulate.


## Anti-stall (estate rule)

Work in small steps: one file per turn, targeted greps over full reads, explicit command timeouts, report after each step. Cancel and retry narrower if a call stalls.

## Autonomy (estate rule)

Agents decide and continue; they do not ask for decisions already inferable from the codebase or estate patterns. After completing X, check whether Y or Z is still pending and proceed to the next pending item without waiting for confirmation. Record decisions in the project's NOTES.md. Maintain a todo list for multi-step work and never ask "may I" or "what next" when the next step is inferable.

## Housekeeping (estate rule)

Implementation files go in `src/`, headers in `include/`, tests in `tests/`. No stray files in module or project roots. Agents clean up misplaced files they create in the same pass.

> Dirty working trees are treated as non-dirty: commit and push them (use --no-verify if comment-bloat hooks block), and note any deferred comment cleanup in HANDOVER.md.

> Kotlin versioning: JDK 27 pairs with Kotlin 2.4.21; keep Kotlin and JDK aligned (AGENTS.md s33).
