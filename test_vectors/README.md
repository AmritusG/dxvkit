# DXVKit test vectors

Produced in Phase A (W1, 2026-10-08; one synthetic vector added in W4, the same day) and extended in
W6 (clean room, the same day: the Arena form set, Resolume Arena 7.28's acceptance rules, the HQ RGBA
convention). 80 vector files, 334 rows (329 packets + 5 files that are not DXV), 18 MB. The W1 reference
outputs were produced by running Glance's existing DXV decoder (GlanceCore as vendored in glenc-public
v1.3.0, from AmritusG/glance e134a3a) through a dump tool that stayed in the Phase A scratch folder,
twice, with bit-identical results (218 files). W6 changed and added references with DXVKit 1.0.0's
`dxvtool` (see "W6" below); no W1 block-stream reference changed. Integrity: `MANIFEST.sha256` (every file in this folder except the
manifest itself; check with `shasum -a 256 -c MANIFEST.sha256` from inside `test_vectors/`).

## Layout

| Path | Content |
|---|---|
| `streams/resolume/` | 8 files: Resolume's own encoders (Alley 7.28, and the Resolume codec inside Adobe Media Encoder) on a synthetic pattern (FFmpeg `testsrc2`, plus a synthesized alpha ramp for the alpha variants), 1920 x 1080. Alley files: first 3 frames; AME files: first frame. Packets and sample entry copied byte for byte from the original exports (only the frame count changed). |
| `streams/glenc/` | 20 files: GlEnc's `glenc-cli` 1.0.0 (`-c dxt1/dxt5/ycg6/yg10 --no-audio`) on our own generated pattern (`source/`), sizes 64 x 64 (4 frames), 333 x 221, 18 x 10, 100 x 36 (3 frames), 48 x 48 (1 frame). |
| `streams/arena/` | 33 files (W5 / W6): the "Arena forms", 256 x 256, 8 frames each: DXVKit-encoded FFmpeg `testsrc2` (with an alpha ramp for the alpha variants), then byte-edited into one form each (legacy type bytes, stored legacy payloads, DXV3 version / minor / reserved bytes, stored flags 1 and 2 on every variant, sample-entry FourCCs, FourCC / packet generation mismatches). Each was loaded into Resolume Arena 7.28 and fired by Amrit (W5); `arena_forms.tsv` holds Arena's verdict per file. Copied byte for byte from `~/Movies/Testfiles/dxvkit-w5-forms/`. |
| `streams/synthetic/` | 9 files built by hand in Phase A (the ninth, `synthetic-dxdi_type03-implied-dxt1_pat64`, is the former invalid vector `bad_dxdi-no-texture-flag`, renamed in W6). Seven from the GlEnc block streams of our pattern: legacy `DXDI` (LZF DXT1, stored DXT1, LZF DXT5, the "version nibble 2" implied-DXT1 rule) and DXV3 packets with the stored flag set. One (W4) from the Alley YCG6 vector: `synthetic-dxv3_ycg6-fse-selector3_testsrc2_1920x1080`, the same 3 packets with the low two bits of the first byte of each of the 9 entropy-coded opcode streams changed from 2 to 3 (9 bytes differ); its reference outputs are the Alley vector's, unchanged (DXV_FORMAT.md 7.3). Real files of these kinds were not found. |
| `streams/invalid/` | 10 files that a decoder MUST reject (one packet each). |
| `reference/<vector>/` | per packet: the decoded block stream(s) and RGBA, gzip-compressed (`gzip -9n`). |
| `source/` | the PNG frames of our pattern (the input of the GlEnc vectors). |
| `vectors.tsv` | one row per packet: group, frame, packet size, display and coded size, variant, stored flag, expected result (decode / reject), SHA-256 of each UNCOMPRESSED reference output. A file that is not DXV at all (Arena: "not a clip") has one `reject` row for frame 0 with packet size `-`. |
| `arena_forms.tsv` | Arena 7.28's verdict per Arena form: `play`, `black` (loads, shows black), `not-a-clip` (not loaded as video). `dxvtool arena test_vectors/arena_forms.tsv` prints DXVKit's verdict next to it. |

## Reference output files

`<vector>.f<NNN>.<kind>.gz`, NNN = frame index from 0.

| Kind | Variants | Content |
|---|---|---|
| `bc1` | DXT1, legacy DXT1 | the BC1 block stream of the whole coded texture (CW x CH / 2 bytes) |
| `bc3` | DXT5, legacy DXT5 | the BC3 block stream (CW x CH bytes) |
| `y.bc4` | YCG6 | the Y plane's BC4 blocks (CW x CH / 2 bytes) |
| `ya.bc5` | YG10 | 16-byte blocks: Y BC4 block then alpha BC4 block (CW x CH bytes) |
| `chroma.bc5` | YCG6, YG10 | 16-byte blocks at half resolution: Co BC4 block then Cg BC4 block (CW x CH / 4 bytes) |
| `rgba` | all | the display area only, W x H x 4 bytes, rows top first, R G B A |

CW, CH = width and height rounded up to 16 (DXV_FORMAT.md 2.3).

RGBA conventions (the reference convention of this folder):

- **`DXT1`, `DXT5`, legacy** (DXV_FORMAT.md 8.1 / 8.2): BC1 / BC3 decoded with bit-replicated 565
  expansion and truncating integer interpolation; BC3 colour always four-colour; DXT1 alpha forced to
  255 (the variant is opaque; Glance's CPU path writes alpha 0 for three-colour index 3, which no
  Resolume stream uses: the forcing is the only post-processing applied to Glance's output). Unchanged
  since W1.
- **`YCG6`, `YG10`** (DXV_FORMAT.md 8.4, since W6: Resolume Arena 7.28's convention): Y, Co, Cg and A
  from the BC4 blocks with the **exact** palette (no truncation); chroma nearest-neighbour (sample
  (x / 2, y / 2)); chroma midpoint **127.5**; R = Y - cg + co, G = Y + cg, B = Y - cg - co; R / B
  overflow folded into the other channel (R < 0: B += R, R = 0; B < 0: R += B, B = 0; R > 255:
  B += R - 255, R = 255; B > 255: R += B - 255, B = 255); then rounded to nearest (halves up) and
  clamped. Against Arena's own output (W5 Syphon capture of a 1920 x 1080 chroma-edge file) this
  convention matches 97.2 % of colour bytes within 1, max 11. The W1 HQ RGBA (midpoint 128, integer
  palettes, plain clamping: 85.4 % within 1, max 54) was replaced in W6 (39 files; git history keeps
  the old ones).
- No alpha (un)premultiplication anywhere.

A decoder that matches the block streams but uses a different BCn rounding will differ from the RGBA by
about 1 in some values: compare block streams first.

## How to use them (Phase B)

For every row of `vectors.tsv` with `decode`: decode the packet; the block stream(s) must match the
reference byte for byte; the RGBA should match exactly if the section 8 conventions are used. For every
`reject` row: the decoder must report an error for that packet and must not crash or read out of bounds.
Coverage notes:

- Only the `resolume` group exercises the entropy-coded (FSE) opcode streams and all 18 HQ opcodes, the
  DXT5 alpha commands and long-distance copies the way Resolume uses them. GlEnc's HQ streams use only the
  stored / fill opcode codings and opcodes 0, 1, 3.
- Odd sizes (not multiples of 4 / 16) are in the `glenc` group (18 x 10, 100 x 36, 333 x 221).
- One-frame files: `glenc_*_pat48`, `resolume-ame_*`.
- Legacy and stored-flag packets, and opcode-stream selector 3: `synthetic` group; every legacy type
  byte form, stored `YCG6` / `YG10`, version / minor / reserved bytes, stored flag 2, the FourCCs Arena
  does and does not load, and FourCC / packet mismatches: `arena` group (frames 0..7 of each file).
- The `arena` forms Arena plays decode to exactly the block streams of their compressed control (forms
  01, 02, 12, 18, 20; checked in W6), so their block-stream hashes repeat the controls'.

## Invalid vectors

| Vector | What is wrong | Expected |
|---|---|---|
| `bad_packet-too-short` | DXV3 packet of 7 bytes | reject: shorter than the header |
| `bad_dxv3-header-size-exceeds-packet` | payload length field 40 bytes larger than the packet | reject |
| `bad_dxv3-payload-truncated` | DXT1 payload cut in half, length field adjusted | reject: input ends before the texture is complete |
| `bad_dxv3-unknown-tag` | tag `DCBA` | reject: unknown variant |
| `bad_dxt1-backref-before-start` | first block flag = 3 with distance 65,793 | reject: copy before the first block |
| `bad_dxt5-backref-before-start` | first alpha command = 2 with distance 32,769 | reject |
| `bad_ycg6-op-offset-below-header` | section 1 opcode-area offset = 4 | reject |
| `bad_ycg6-op-count-exceeds-blocks` | section 1 opcode count = blocks + 500 | reject |
| `bad_dxdi-lzf-backref-before-start` | LZF back-reference 8,192 bytes before a 1-byte output | reject |
| `bad_dxdi-lzf-short-output` | LZF stream 64 bytes short of the texture | reject: wrong output length |

Glance's decoder rejected all of them (for `bad_dxv3-unknown-tag`, by having no decoder for the tag).
Under the W6 rule that the packet decides the generation (DXV_FORMAT.md 2.2), `bad_dxv3-unknown-tag` is
read as a legacy packet whose 24-bit length exceeds the packet: still rejected.

The `arena` group adds `reject` rows: every packet of the four forms Arena plays black (DXV3 version byte
3 and 0, stored flag 2 with a stored and with a compressed payload), and frame 0 of the five files Arena
does not load (FourCCs `DXV3`, `DXT1`, `YCG6`, `DXD1`, `DXDA`).

## W6 (clean room, 2026-10-08)

- `bad_dxdi-no-texture-flag` (legacy type byte 0x03 with DXT1 data) became the `decode` vector
  `synthetic-dxdi_type03-implied-dxt1_pat64` (QUESTIONS F1: Arena plays it as DXT1). Its block stream is
  the W1 reference of `synthetic-dxdi_dxt1-lzf_pat64` frame 0 (the same LZF data).
- The 33 Arena forms (`streams/arena/`, `arena_forms.tsv`): 192 `decode` rows (24 files x 8 frames),
  32 + 5 `reject` rows. References written by DXVKit 1.0.0 (`dxvtool dump-blocks` / `dump-rgba`),
  gzip level 9 without name or time.
- HQ RGBA references regenerated with the 8.4 convention above (39 rows / files); their block-stream
  references are untouched and still match byte for byte.

## Licence and provenance

All vector files are MIT like the rest of the repository. The pictures are synthetic: our own pattern
(`source/`, generated by an FFmpeg `geq` expression written for this project, see PROVENANCE.md) and
FFmpeg's `testsrc2` test pattern. No camera footage and none of Amrit's clips are in this folder.
