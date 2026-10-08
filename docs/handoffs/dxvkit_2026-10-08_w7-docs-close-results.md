project:    dxvkit
pass:       w7-docs-close
STATUS:     COMPLETE
parent:     8e9ca73ef7904f257381f9e3b111c412214b11e6
result:     7a033fad8e1acdb7d63988f1248daa738fcd1867
session:    10fd4aad-1f65-4b80-ad18-ce295f2e32d4
written_at: 2026-10-08T19:27:45Z

# DXVKit W7 results: docs close (Phase A, docs only)

## What shipped
- **W6-2 closed with Amrit's answer, verbatim: "Keep exact."** ENCODING_NOTES 5 now states the decided
  policy: the HQ encoder writes exact, unrounded Y / Co / Cg (midpoint 127.5, 2 x 2 box chroma), as
  v1.0.0 does and as Alley does. Integer Y was measured in W6 at -4.6 % size (126,236 vs 132,336 B) and
  -0.08 dB (37.82 vs 37.90 dB) and is recorded as declined.
- **W6-1 closed.** The three sentences describing pre-1.0.0 DXVKit now match v1.0.0 and F1-F7:
  DXV_FORMAT.md 4.1 (the 0x03 vector is named as reclassified to a decode vector, F1); 8.4 (the
  reference RGBA follow 8.4 since W6: 97.21 % within 1, max 11 vs Arena; W1 convention kept as history,
  F6); ENCODING_NOTES 5 (the "finding for the clean room" paragraph became the policy above, F7).
  Revision notes added to both spec headers.
- **Remaining open-sounding items settled from existing evidence:** QUESTIONS 7 ("still Amrit's open
  question") and 20 ("For Amrit") were answered in W4 (W4-4, W4-6); their cells now say so. F7's
  "Arena capture of the new file optional" is recorded as an optional check, not an open question.
  **Open count: 0.**
- PROVENANCE.md: W7 Phase A addendum (read only repo docs/handoffs; no upstream source, no measurements).

## Validated
- Self-check over docs/spec: identifier hits 0, code fences 0 / 0, line refs 0.
- dxvtool verify unchanged: 334 rows, 366/366 block streams, 287/287 RGBA, 47/47 rejected, 0 failures.
- No code, vectors or tag touched; v1.0.0 still points at 2f5256a (Glance's pin unaffected).

## Deviations
None. The brief's item 1 and 2 both touch ENCODING_NOTES 5; one paragraph serves both.

## For Amrit
Nothing required. Optional, unchanged from W6: an Arena Syphon capture of a v1.0.0 HQ file to confirm
grey 128 shows as 128 / 128 / 128 (WHAT: capture; HOW: play any DXVKit 1.0.0 YCG6 grey clip; WHERE: Arena).

## Commits
- parent 8e9ca73 (W6 results handoff); result 7a033fa (docs), plus the handoff commit that adds this file.

## RECEIPTS
```
$ git log -1 --format='%H %an %ad %s' --date=iso
7a033fad8e1acdb7d63988f1248daa738fcd1867 Amrit Rosell 2026-10-08 21:27:14 +0200 DXVKit W7 (Phase A, docs only): close W6-1 and W6-2
$ git ls-remote origin main refs/tags/v1.0.0 'refs/tags/v1.0.0^{}'
7a033fad8e1acdb7d63988f1248daa738fcd1867	refs/heads/main
2aa313aff45d66e334097202913531938e48e9c5	refs/tags/v1.0.0
2f5256ab2c2b10bdfebe68275129dbf51e48fd10	refs/tags/v1.0.0^{}
$ git diff --stat 8e9ca73 7a033fa
 docs/cleanroom/PROVENANCE.md | 14 ++++++++++++++
 docs/cleanroom/QUESTIONS.md  | 19 +++++++++++++++++--
 docs/spec/DXV_FORMAT.md      | 16 ++++++++++------
 docs/spec/ENCODING_NOTES.md  | 16 ++++++++++------
 (+ docs/handoffs/dxvkit_2026-10-08_w7-docs-close-brief.md, new)
$ build/dxvtool verify test_vectors/vectors.tsv | tail -1
rows 334 | block streams 366/366 identical | rgba 287/287 identical | rejected 47/47 | failures 0
$ self-check docs/spec (W5 identifier list)
identifier hits: 0
code fences: 0 0
line refs: 0
$ grep 'Open items after W7' docs/cleanroom/QUESTIONS.md
Open items after W7: none.
```
END OF HANDOFF w7-docs-close sha256:518ad87b 3121
