project:    dxvkit
pass:       w4-spec-cleanup-results-r4
STATUS:     COMPLETE
parent:     8eed207d44e75a5f2e69fbb4bd71563e9c5e6f0e
result:     3641ead9c3203892477805155f9e0d24682377a1
session:    ba6ede15-2402-4047-b5fd-4dca996dcc5c
written_at: 2026-10-08T16:13:01Z

# DXVKit W4 results r4: stored-flag playback confirmed

Follow-up to `dxvkit_2026-10-08_w4-spec-cleanup-results.md`, `-r2`, `-r3` (all still valid).

## Amrit's answer (this session, verbatim)
"Clip 9 and 10 plays perfectly." (Arena 7.28, layer 6 "DXVKit Encoder": DXVKit's 333 x 221 DXV3
stored-flag DXT1 and DXT5 files.)

## What changed
- `DXV_FORMAT.md` 10.2: Arena plays DXV3 stored-flag DXT1 / DXT5 packets; still undefined for YCG6 / YG10.
- `ENCODING_NOTES.md` 2: stored packets confirmed playable in Arena; compressed form preferred, stored a
  valid fallback (the old sentence there was also garbled; rewritten).
- `QUESTIONS.md` 14 settled, W4-8 closed. The encoder option stays off by default; no code change.

## Still open (none needs Amrit)
- Q13 (FFmpeg's BC1 rounding, a third-party reader), spec 10.1-10.4 (no such files exist), 10.6 / 10.7
  (Resolume internals).

## RECEIPTS
```
$ git log -1 --format='%H %s'
3641ead9c3203892477805155f9e0d24682377a1 W4: Arena plays DXV3 stored-flag DXT1/DXT5 (Amrit); QUESTIONS 14 / W4-8 closed
$ git ls-remote origin main
3641ead9c3203892477805155f9e0d24682377a1	refs/heads/main
```
Docs only; Arena and Wire not touched by this session.
END OF HANDOFF w4-spec-cleanup-results-r4 sha256:981cdf2d 1195
