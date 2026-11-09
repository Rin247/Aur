# Integrity and Anti-Cheat

> Source-derived split of the authoritative design archive at `Bible/Archive/Source of Truth/chat-Chrono Trigger Inspired World Bible.txt` (original lines 248-265). Preserve the distinction between user-confirmed direction and assistant proposals; proposed mechanics are not locked without user confirmation.

---

## 10. ANTI-CHEAT & ANTI-PIRACY PHILOSOPHY

**Design Principle:** Never crash. Never accuse. **Gaslight.**

| Layer | Mechanism | Pirate Experience |
|-------|-----------|-------------------|
| **Memory Obfuscation** | SecureInt (XOR encryption + key rotation) | Cheat Engine sees garbage values. |
| **Honey Pot** | If HP jumps by >200 in one frame, flag `is_compromised` | Silent trigger. No notification. |
| **Cursed RNG** | Damage reduced 10–30%. Crits heal enemies. | "This game is really hard." |
| **UI Gaslight** | Text misspells. Health bars drift. Tooltips lie. | "Is this a bug?" |
| **Audio Detune** | Master bus pitch slowly drops as corruption_level rises. | Soundtrack sounds "off." |
| **Beacon Misdirection** | Waypoint beacons spawn 50m from actual objective. | "The guide is wrong." |
| **Ambush Spikes** | Resting has 20% ambush chance (0% for legitimate players). | "Sleeping is risky." |

**Legitimate players never see any of this.** The game plays perfectly clean.

---

