project:    dxvkit
pass:       w4-spec-cleanup-results-r3
STATUS:     COMPLETE
parent:     d7cd3bc5ad1d3b7df7585150d9cc4aace8980afe
result:     429cb439b6f4bba78ae3555d62e2652fc9436a2d
session:    ba6ede15-2402-4047-b5fd-4dca996dcc5c
written_at: 2026-10-08T16:11:37Z

# DXVKit W4 results r3: alpha default decided

Follow-up to `dxvkit_2026-10-08_w4-spec-cleanup-results.md` and `-r2` (both still valid).

## Amrit's answer (this session, verbatim)
"Keep straight as the default, like Alley. DXVKit is for Glance, CueGlance, MaCrate and GlEnc..."

## What changed
- `DXV_FORMAT.md` 8.5 writers table, DXVKit row: default straight recorded as Amrit's decision, with the
  reason. `QUESTIONS.md` W4-4: decision recorded. No code change (the encoder already defaults to straight).
- Nothing left open from W4 except W3's stored-flag playback check (columns 9 / 10, layer 6 in Arena).

## RECEIPTS
```
$ git log -1 --format='%H %s'
429cb439b6f4bba78ae3555d62e2652fc9436a2d W4: record Amrit's decision: DXVKit encoder alpha default stays straight
$ git ls-remote origin main
429cb439b6f4bba78ae3555d62e2652fc9436a2d	refs/heads/main
```
Docs only; Arena and Wire not touched.
END OF HANDOFF w4-spec-cleanup-results-r3 sha256:63ff1952 903
