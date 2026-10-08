project:    dxvkit
pass:       w4-spec-cleanup-results
STATUS:     COMPLETE
parent:     1ce64ccc41283313d1d32ef111856a78286f5995
result:     517c5f433ae0d9416519765ac88945cee4aadbc4
session:    ba6ede15-2402-4047-b5fd-4dca996dcc5c
written_at: 2026-10-08T15:41:56Z

# DXVKit W4 results: Phase A (dirty room) spec cleanup

## What shipped
- Spec revised (prose and tables only): `DXV_FORMAT.md` 2.3, 7.3 (+ new 7.3.4), 7.3.1, 7.5 (rewritten), 8.2,
  8.5 (rewritten), 10; `ENCODING_NOTES.md` 4, 7.1, 7.3, 8. `QUESTIONS.md`: W4 section (W4-1..9), status cells
  of items 1, 3, 10, 11 closed. `PROVENANCE.md`: Phase A W4 entry.
- One new vector: `synthetic-dxv3_ycg6-fse-selector3_testsrc2_1920x1080` (Alley YCG6 vector, 9 selector
  bytes 2 -> 3; expected outputs = the Alley vector's). vectors.tsv +3 rows, MANIFEST, README updated.

## Items (all from measurements on this Mac's DXV files, read as data; none copied)
1. **Dangling HQ opcode** -> closed, spec 7.5. 142 Resolume HQ files / 45,980 packets: 43,707 of 43,950
   YCG6 packets carry exactly ONE extra opcode, always in Y, never chroma; 0 of 2,030 YG10. New: it codes
   one block PAST the Y plane, equal to Co block 0 (checked by decoding it on 8 frames; opcodes 3, 2, 7).
   Exceptions = the one 512x1280 file (no padding, 241 packets) + one packet in each of two clips. 7.5 now
   has an encoder / decoder legality table; W1 finding 2 corrected in the spec (handoff left as written).
2. **Selector 3** -> closed, spec 7.3.4: decode like 2, write 2. 0 of 139,970 Resolume streams use it.
   FFmpeg 8.1.1 black box: selector-3 copies of Alley YCG6+YG10 (21 streams, 6 frames) bit-identical;
   control (-> 0) rejected. Also: Resolume never writes fill (stored for 1-3-opcode streams, 1,319);
   single-symbol entropy tables are unwritable (spec 10.8 was wrong; fixed).
3. **BC3 endpoints** -> closed, spec 8.2: writers MUST order four-colour (c0 > c1, or c0 = c1 without
   index 3); readers SHOULD use four-colour. Resolume DXT5: 298 files x 2 frames, 78,358,368 colour halves,
   c0 < c1 in **0**; c0 = c1 in 8,792,383, none with index 3 -> BC1-rule readers (FFmpeg) are fine on
   Resolume files. One GlEnc cluster-fit experiment file has c0 < c1 in 99.96 % of blocks.
4. **Alpha** -> recorded, spec 8.5: writers table (Alley straight 97.4 %; AME codec premultiplied-looking
   0.1 %; GlEnc either, by version / variant; Resolume Wire doc clips straight), readers table (FFmpeg
   un-premultiplies DXT5; Arena per clip; DXVKit untouched by default). No field in the file. 360 alpha
   files measured, 329 unclassifiable (too few semi-transparent pixels).
5. **Other open items**: Q17 confirmed (W4-5), Q20 superseded by 21/22 (closed). Stay open: Q13 FFmpeg BC1
   rounding (third-party reader, not the format); Q14 Arena playback of stored-flag files (Amrit); spec
   10.1-10.4 (no such files exist), 10.6 / 10.7 (Resolume internals, unmeasurable here).

## Deviations
- **No upstream source was read.** The brief allowed FFmpeg / Glance source; the Claude Code permission
  layer refused the GitHub fetch and the local `~/Downloads/dxv.c` read, and I did not work around it.
  Item 2 was settled black box instead; Q13 stays open for that reason. Recorded in PROVENANCE.
- Verify counts are 133/133, 94/94, 11/11 (brief said 127/91/11) because of the new vector's 3 rows; the
  127 + 91 original rows are unchanged.
- The file set is Spotlight-based (674 DXV files, excluding DXVKit outputs / vector copies), so counts
  differ from W1's walk (e.g. 134 Resolume YCG6 files vs 157).
- I read ~100 lines of `src/hq.c` (our own MIT decoder) to understand `dxvtool check`; recorded.

## Findings for the next clean-room wave
- None shows the code wrong. Suggested: add the new rule from spec 8.2 ("c0 = c1 must not use index 3")
  to the encoder's self-check; optionally let `dxvtool check` name the channel of a dangling opcode.

## Needs Amrit
- WHAT: confirm the name and default of Arena's per-clip alpha setting. HOW: open any alpha clip's
  properties in Arena and read the alpha / premultiply option. WHERE: Arena 7.28, clip panel. (Its web API
  doesn't expose it; spec 8.5 quotes the brief until then.)
- WHAT: still open from W3: trigger columns 9 / 10 of layer 6 "DXVKit Encoder" (stored-flag files).
- No questions were asked of Amrit in this session (no answers to record).

## RECEIPTS
```
$ git log -1 --format='%H %s'
517c5f433ae0d9416519765ac88945cee4aadbc4 DXVKit W4 (Phase A, dirty room): spec cleanup of the open questions
$ git ls-remote origin main
517c5f433ae0d9416519765ac88945cee4aadbc4	refs/heads/main
$ ./build/dxvtool verify test_vectors/vectors.tsv --threads 0 | tail -1
rows 105 | block streams 133/133 identical | rgba 94/94 identical | rejected 11/11 | failures 0
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256) | grep -c ': OK$'    (290 entries; 0 not OK)
290
$ self-check docs/spec: upstream-identifier grep (dxv_ op_ AV_ avctx bytestream CHECKPOINT cgo cocg tab0-3
  val1/2 lshift rshift old_type version_major ff_ AVERROR size_in_bits get_bits decompress_texture
  fill_ltable fill_optable lzf_uncompress texdsp + Glance type names) / code fences / line refs
identifier hits: 0   code fences: DXV_FORMAT.md 0, ENCODING_NOTES.md 0   line refs: 0
$ dxvtool check, 142 Resolume HQ files (summary line)
files 142 (bad 0) | packets 45980 | errors 0 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 43707
$ BC3 colour halves, 298 Resolume DXT5 files x 2 frames
blocks 78358368 c0>c1 69565985 c0==c1 8792383 eq_idx3 0 c0<c1 0
$ ffmpeg -f framemd5, Alley YCG6 original vs selector-3 copy (frames 0-2, same for YG10)
88337dbf04a21824feed0a9e67ed75fa 6cef6ea5df16feaab998ab84ffbc7666 e7e6d0097fafe45f44cfb42525ac80df (both)
$ ls -la test_vectors/streams/synthetic/synthetic-dxv3_ycg6-fse-selector3_testsrc2_1920x1080.mov
-rw-r--r-- 357581 Oct 8 17:37
```
Arena: read-only (clip list + one clip's parameters); nothing loaded, triggered or saved. Wire not touched.
END OF HANDOFF w4-spec-cleanup-results sha256:ccd551a1 5721
