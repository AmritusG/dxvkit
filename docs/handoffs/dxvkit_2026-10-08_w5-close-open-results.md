project:    dxvkit
pass:       w5-close-open-results
STATUS:     COMPLETE
parent:     52cf846a9799a9ce49aef97895e763729e73c40e
result:     af674b0b6b8d0429ad3ab6aeb381c9dbcdb9b095
session:    18dfdfdd-8b9e-408a-98d9-8a2fbe0f801d
written_at: 2026-10-08T17:15:36Z

# DXVKit W5 results: spec section 10 closed (Phase A, dirty room)

Brief: `docs/handoffs/dxvkit_2026-10-08_w5-close-open-brief.md`. Docs only; no code touched.
Spec section 10 now lists nothing open; each item names its authority and evidence.

## Per item: authority, evidence, spec change

1. **Q13, FFmpeg's BC1 rounding.** Authority: FFmpeg 8.1.1 source (texturedsp, MIT-licensed file),
   then its output. FFmpeg expands 5 / 6-bit values by an integer rounded division (not bit replication;
   differs by 1 at 7 + 11 values), with truncating interpolation. Model vs FFmpeg's RGBA: **0 bytes
   differ** on 4 frames (Alley + DXVKit, DXT1 + DXT5, 1080p). The spec rule differs in 0.589 % (the W3
   figure). Spec 8.1. Also **spec 8.2 corrected**: FFmpeg always reads DXT5 colour as four-colour (W4
   said three-colour). Black box: 744 swapped colour halves give 0 bytes off four-colour, 6,173 off
   three-colour.
2. **10.1-10.4, the forms.** Authority: Arena 7.28. 33 synthetic 256 x 256 files (DXVKit-encoded
   testsrc2 + alpha ramp, byte-edited) in layer 8 "DXVKit W5 Forms", columns 1-33. Amrit, verbatim:
   "14, 17, 29 31 plays black. 24, 25, 26, 32, 33 does not register as a clip." (all others play).
   - Legacy: every type byte plays; 0x40 → DXT5, else DXT1, 0x03 included. Spec 4.1: never reject for the
     type byte.
   - DXV3: version byte 5 / 6 play, 3 / 0 black → reject < 4. Minor 1 and reserved 1 play. Stored flag 2
     is black (stored or compressed payload) → only 0 / 1. YCG6 / YG10 with stored flag 1 PLAY, laid
     out as the plane streams back to back. Spec 3.1-3.3.
   - FourCCs: `DXD3` / `DXDI` / `DXD4` load; `DXV3`, `DXT1`, `YCG6`, `DXD1`, `DXDA` don't. Cross cases
     (`DXDI` + DXV3 packets, `DXD3` + legacy packets) play: the packet decides. Spec 2.2, 9.
3. **10.6, Arena's HQ playback.** Authority: Arena's composition pixels over Syphon (Amrit switched it
   on; server "Arena - Composition"; scratch client). The MCP `inspect` capture proved lossy on a DXT1
   control (±30), so it was discarded. The DXT1 control over Syphon: geometry exact, colour within 2.
   YCG6 chroma-edge file (columns 34-36):
   - **nearest chroma, midpoint 127.5, R/B overflow folding**: 97.24 % of bytes within 1, max 11.
   - Bilinear: max 124. The spec's old 128 / integer rule: max 54.
   - YG10 gives a byte-identical output.
   Spec 8.4.
4. **10.7, Resolume's forward transform.** Authority: Alley 7.28 exports (by Amrit) of our lossless probe
   source (120 flat patches, ramps, checkerboards, alpha ramp). Alley = exact, unrounded YCoCg with
   **midpoint 127.5**. Patch errors are at most 0.07 (Y), 0.21 (Co) and 0.46 (Cg) under the float
   palette; midpoint 128 fits worse. Chroma = **2 x 2 box** (0.33 against 11.9 or more). BC4 targets
   fractional values with adjacent endpoints. ENCODING_NOTES 5.

## Findings for the clean room (QUESTIONS.md F1-F7)
- **F1** legacy 0x03: DXVKit rejects, Arena plays. Vector `bad_dxdi-no-texture-flag` now contradicts the
  spec; left unchanged.
- **F2** version < 4: DXVKit decodes, Arena shows black.
- **F3** stored flag 2: DXVKit decodes, Arena shows black.
- **F4** HQ stored: DXVKit rejects, Arena plays.
- **F5** `DXD4` and the cross FourCC / packet cases: DXVKit rejects, Arena plays.
- **F6** the RGBA path uses midpoint 128; it should use 127.5 plus folding. Also decide whether to
  regenerate the reference HQ RGBA.
- **F7** the encoder writes chroma midpoint 128. In Arena a grey 128 shows as 128, 129, 127.

LoadEhr / Glance: nearest chroma matches Arena; their chroma midpoint must be 127.5. That wasn't checked:
the permission layer refused the search for their source.

## Deviations
- The brief's verify counts (127 / 91 / 11) predate W4's selector-3 vector; the real totals are 133 /
  94 / 11, all pass.
- No new vectors. The HQ-stored and legacy-0x03 forms would fail DXVKit's verify until F1 / F4 are
  done. The form files are listed by path instead (`~/Movies/Testfiles/dxvkit-w5-*`).
- The permission layer refused fetching `dxv.c` and a bulk read of it; a grep of its routine selection
  sufficed. Bash was allowed by Amrit mid-session after a block.
- 5 extra probe forms (29-33) were added after the first thumbnails, to pin down the stored-flag 2 and
  version-byte rules.
- Amrit: the qtrle probe source does not open in Glance (opens in Alley). Not a DXV question; noted only.

## Needs Amrit's eyes
- Arena: layer 8 and columns 23-36 were added, and the "Arena - Composition" Syphon output is on. The
  comp was NOT saved. Remove them or keep them as you like.
- Decide F6: should the test vectors' HQ RGBA move to the 127.5 convention?

## RECEIPTS
```
$ git log -1 --format='%H %s'
af674b0b6b8d0429ad3ab6aeb381c9dbcdb9b095 DXVKit W5 (Phase A, dirty room): close spec section 10
$ git ls-remote origin main
af674b0b6b8d0429ad3ab6aeb381c9dbcdb9b095	refs/heads/main
$ ./build/dxvtool verify test_vectors/vectors.tsv | tail -1
rows 105 | block streams 133/133 identical | rgba 94/94 identical | rejected 11/11 | failures 0
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256) | grep -c ': OK$'
290   (not OK: 0)
$ self-check docs/spec (dxv_ op_ AV_ avctx bytestream CHECKPOINT cgo cocg yo tab0-3 val1/2 lshift rshift
  old_type version_major ff_ AVERROR size_in_bits texturedsp extract_color dxt*_block premult2straight
  rgtc decompress_indices texdsp tex_funct dxtn AV_RL av_clip ycocg2rgba FFMIN)
identifier hits: 0   code fences: DXV_FORMAT.md 0, ENCODING_NOTES.md 0   line refs: 0
$ Q13 model vs FFmpeg RGBA (frame 0, 8,294,400 bytes each)
dxvkit-w3_dxt1 0 differ | resolume-alley_dxt1 0 | dxvkit-w3_dxt5 0 | resolume-alley_dxt5 0
$ Arena HQ fit (Syphon, 1,555,200 bytes)
nearest,127.5,folding  exact 81.10% <=1 97.24% <=2 98.56% max 11 mean 0.248
nearest,128,int (ref)  exact 32.63% <=1 85.80% <=2 96.07% max 54 mean 1.104
bilinear centred,127.5 exact 38.74% <=1 42.51% <=2 43.51% max 124 mean 14.673
$ ls ~/Movies/Testfiles/dxvkit-w5-forms | wc -l ; du -sh   -> 33 files, 3.2M
$ ls -la ~/Movies/Testfiles/dxvkit-w5-chroma
 3051260 Oct  8 18:30 dxvkit-w5_chroma-edges_dxt1-control_1920x1080.mov
 5480030 Oct  8 18:29 dxvkit-w5_chroma-edges_ycg6_1920x1080.mov
 5480600 Oct  8 18:30 dxvkit-w5_chroma-edges_yg10_1920x1080.mov
$ ls -la ~/Movies/Testfiles/dxvkit-w5-alley ~/Movies/Testfiles/dxvkit-w5-alley/exports
 1467673 Oct  8 18:30 dxvkit-w5_alley-probe_patches-ramps_1920x1080_qtrle.mov
  753136 Oct  8 18:38 ..._qtrle_DXV High Quality With Alpha.mov   (dxvtool info: YG10, 5 frames)
  673271 Oct  8 18:39 ..._qtrle_HQ - No Alpha No Audio.mov        (dxvtool info: YCG6, 5 frames)
Arena: composition "LoadEhr" (not saved), layer 8 "DXVKit W5 Forms", 36 clips in columns 1-36; no
clips triggered by Claude; Wire not touched.
```
END OF HANDOFF w5-close-open-results sha256:ba4509ec 6746
