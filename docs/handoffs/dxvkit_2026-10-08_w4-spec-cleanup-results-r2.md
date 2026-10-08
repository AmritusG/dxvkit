project:    dxvkit
pass:       w4-spec-cleanup-results-r2
STATUS:     COMPLETE
parent:     517c5f433ae0d9416519765ac88945cee4aadbc4
result:     a89241139b02368ac0169c1c607f6032f6279d6d
session:    ba6ede15-2402-4047-b5fd-4dca996dcc5c
written_at: 2026-10-08T16:05:15Z

# DXVKit W4 results r2: Arena "Alpha Type" recorded

Follow-up to `dxvkit_2026-10-08_w4-spec-cleanup-results.md` (still valid; this adds Amrit's answer).

## Amrit's answer (this session, verbatim)
"Arena's alpha option is called "Alpha Type", default Premultiplied"

## What changed
- `DXV_FORMAT.md` 8.5 readers table: Arena 7.28 = per-clip "Alpha Type", default Premultiplied, so by
  default Arena reads every alpha file as premultiplied (like FFmpeg's DXT5 decoder); an Alley (straight)
  file needs "Alpha Type" switched to composite as intended. Reader rule: let the user choose per clip;
  match Arena by defaulting to premultiplied. Spec 10.5 closed (the file cannot say).
- `ENCODING_NOTES.md` 4: still write straight (like Alley), and tell users of straight files to set
  "Alpha Type" in Arena.
- `QUESTIONS.md` W4-4: closed.

## Open / needs Amrit's eyes
- Writer default: the spec keeps "write straight, like Alley", although Arena's default reading is
  premultiplied, so a straight DXVKit or Alley file looks wrong in Arena until "Alpha Type" is changed.
  If you'd rather DXVKit match Arena's default, the encoder's default would become premultiplied (a
  clean-room code change, not done here). WHAT: decide straight vs premultiplied as DXVKit's encoder
  default. HOW: tell the next session. WHERE: any session.
- Still open from W3: trigger columns 9 / 10 of layer 6 "DXVKit Encoder" (stored-flag playback).

## RECEIPTS
```
$ git log -1 --format='%H %s'
a89241139b02368ac0169c1c607f6032f6279d6d W4: Arena's per-clip "Alpha Type" (default Premultiplied) into spec 8.5 / 10.5, QUESTIONS W4-4 closed
$ git ls-remote origin main
a89241139b02368ac0169c1c607f6032f6279d6d	refs/heads/main
$ ./build/dxvtool verify test_vectors/vectors.tsv --threads 0 | tail -1
rows 105 | block streams 133/133 identical | rgba 94/94 identical | rejected 11/11 | failures 0
$ grep -c code-fences docs/spec/*.md
DXV_FORMAT.md 0, ENCODING_NOTES.md 0
```
Docs only; Arena and Wire not touched.
END OF HANDOFF w4-spec-cleanup-results-r2 sha256:f6c20267 1989
