```
DXVKIT W4 — PHASE A (DIRTY ROOM) SPEC CLEANUP: CLOSE THE OPEN SPEC QUESTIONS FROM THE DECODER AND ENCODER WAVES — DOCS ONLY, RUN TO FINISHED

MODE: docs-only wave on DXVKit (~/Documents/claude/DXVKit). This session acts as PHASE A (the dirty room) under DXVKit's HARD RULES: it MAY read FFmpeg dxv.c / dxvenc.c / texturedsp*.c and Glance / GlEnc source, and writes ONLY prose, tables and test vectors: no code, no upstream identifiers, no line references. A fresh session (/clear first); do not reuse the encoder session's context (it was a clean-room session and must stay clean in the record).
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"Clean room. Can it be made to work / merge/ replace Glance and the others you built?"
(Rule he set: a backlog is not finished work: clear it, validated.)

THE OPEN ITEMS (from docs/cleanroom/QUESTIONS.md; settle each, then mark it closed there with the evidence)
1. W2 item 1: the dangling HQ opcode tail (Resolume YCG6 frames leave one opcode + 8 literal bytes unread in the Y channel; 156,100 of 161,737 real packets). W1 finding 2 said "every Resolume stream uses every opcode and literal exactly": correct that wording and sharpen spec 7.5 so an encoder and a decoder both know what is legal.
2. W2 item 3: opcode selector 3 never occurs in any vector or real file; Phase B treats it like 2. Confirm from the upstream semantics what 3 means (or that it is invalid) and write it into the spec.
3. W3 item 10: BC3 colour endpoints: readers that apply the BC1 three-colour rule when c0 <= c1 decode differently; the encoder now orders c0 > c1. Add to spec 8.2 what a writer must do and what a reader should expect from Resolume's files (measure Resolume's DXT5 files: do they ever have c0 <= c1?).
4. W3 item 11 + the alpha question: record in the spec, as a table, which writers produce straight vs premultiplied-looking colour (Alley straight, Adobe Media Encoder plugin premultiplied-looking, ffmpeg's decoder un-premultiplies DXT5), and that the file carries no field for it; Arena makes the user choose per clip.
5. Anything else still marked open in QUESTIONS.md: list it and settle it the same way, or say why it stays open.

RULES FOR THE WRITE-UP
- Prose and byte tables only; run the W1 self-check (upstream identifier grep = 0 hits, no code fences, no line refs) and paste the receipt.
- Where a fact comes from measuring real files, give the count and the files' folders (no clips copied).
- Add test vectors only if a settled item needs one (our own synthetic patterns; MANIFEST updated).
- Do not touch src/, include/, swift/, tools/ (the code belongs to the clean room). If a settled item shows the CODE is wrong, write it as a finding for the next clean-room wave instead of fixing it.

BUILD + TEST
1. Nothing to build. The vectors still verify (dxvtool verify: 127/127, 91/91, 11/11) and the manifest hashes match.
2. Commit + push. Handoff docs/handoffs/dxvkit_2026-10-08_w4-spec-cleanup-results.md, cp -p to ~/claude-handoffs/: each item, what was found, where the spec changed, items left open (with why), any finding for the clean room, the self-check receipt, Amrit's answers from this session.

STANDING RULES
One Claude per folder (no other session in DXVKit while this runs). Cowork runs no git. Licences MIT / BSD / Apache / zlib / ISC / Boost only. No footage that is not ours in the repo. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
