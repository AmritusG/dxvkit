```
DXVKIT W7 — DOCS ONLY: RECORD AMRIT'S W6-2 ANSWER AND FIX THE W6-1 SPEC SENTENCES (PHASE A) — RUN TO FINISHED

MODE: docs-only wave on DXVKit (~/Documents/claude/DXVKit), acting as PHASE A under the HARD RULES (prose / tables only; no code in src/ include/ swift/ tools/; no upstream identifiers or line refs). Fresh session (/clear first). Runs in parallel with the Glance session (another folder; Glance pins tag v1.0.0, which this wave does NOT move or retag). Commit + push; no new tag.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
On QUESTIONS W6-2 (exact Y vs integer Y in the HQ encoder): "Keep exact."

DO
1. QUESTIONS.md W6-2: closed with his answer verbatim; the encoder keeps exact unrounded Y / Co / Cg (v1.0.0 behaviour). ENCODING_NOTES 5: state it as the decided policy (Alley-like; integer Y measured at -4.6 % size / -0.08 dB and declined).
2. QUESTIONS.md W6-1: correct the three spec sentences that still describe DXVKit's pre-v1.0.0 behaviour so DXV_FORMAT.md matches v1.0.0 and Arena's rules (F1-F7). Close W6-1.
3. Anything else in QUESTIONS.md still marked open: list it; settle it from the existing evidence or state why it stays open. Target: nothing open.
4. Self-check receipt (identifier grep 0, no code fences, no line refs). dxvtool verify unchanged (all rows pass).
5. Handoff docs/handoffs/dxvkit_2026-10-08_w7-docs-close-results.md, cp -p to ~/claude-handoffs/: what changed, open count (expected 0), Amrit's answers.

STANDING RULES
One Claude per folder. Cowork runs no git. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
