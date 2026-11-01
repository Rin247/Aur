# Aur Bible

This directory is the maintained home for Aur's design, production, and project documentation. Start with the operational handover and roadmap when working on the MVP; consult the focused bibles for design intent.

## Source of truth

`Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` is the user's designated authority for the original world and game-design material. The numbered documents under `World/`, `Game Design/`, `Systems/`, `Characters/`, `Time and Puzzles/`, and `Production/` are line-cited extracts of that conversation, reorganized by topic. Later user-confirmed clarifications are labeled in the relevant documents and take precedence over earlier or assistant-generated material where they conflict; a proposal is not a locked decision unless the user confirmed it.

When sources conflict, use this precedence:

1. User-confirmed decisions in the Source of Truth archive and later user-confirmed clarifications recorded in the focused bibles.
2. Current implementation and project data for what is actually implemented.
3. Focused Bible extracts for organized access to design material.
4. `Archive/Legacy/` for historical context only.

## Current project docs

- [Handover](Operations/Handover.md) — implementation status and current handoff.
- [MVP roadmap](Operations/Roadmap.md) — outstanding work and priorities.
- [Project overview](Operations/Project%20Overview.md) — product, architecture, and verified features.
- [Agent guide](Operations/Agent%20Guide.md) — repository constraints and working practices.
- [MCP server](Operations/MCP%20Server.md) — local agent integration and project data.

## Design and production

- [Game Design](Game%20Design/) — core vision, clock, classes, combat, social systems, audio, integrity, accessibility.
- [World](World/) — cosmology, peoples, mana, geography, language, biology, eras, and visual motifs.
- [Characters](Characters/) — protagonists, recruitment, and solo-play continuity.
- [Character creation, sprite art, and animation](Production/Character%20Creator%20and%20Species%20Visuals.md) — full cast coverage and visual direction.
- [Systems](Systems/) — corruption, creed, and paradox/integrity consequences.
- [Time and Puzzles](Time%20and%20Puzzles/) — era shifts, puzzle design, memory dives, and causal puzzles.
- [Production](Production/) — technical targets and production-facing audio/UI references.
- [Archive](Archive/) — immutable source material and retired documents retained for provenance.

## Editorial rules

- Keep lore, game mechanics, and production/technical guidance in their respective areas rather than combining unrelated topics.
- Update focused documents alongside implementation when a confirmed decision changes.
- Preserve the source archive unchanged; edit the organized extracts only when correcting extraction metadata or reflecting a confirmed source change.
- Treat legacy documents as historical references, not competing current specifications.
