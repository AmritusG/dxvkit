# DXVKit test vectors

Produced in Phase A (W1, 2026-10-08). 46 vector files, 102 packets, 11 MB. Every reference output was
produced by running Glance's existing DXV decoder (GlanceCore as vendored in glenc-public v1.3.0, from
AmritusG/glance e134a3a) through a dump tool that stayed in the Phase A scratch folder, twice, with
bit-identical results (218 files). Integrity: `MANIFEST.sha256` (every file in this folder except the
manifest itself; check with `shasum -a 256 -c MANIFEST.sha256` from inside `test_vectors/`).

## Layout

| Path | Content |
|---|---|
| `streams/resolume/` | 8 files: Resolume's own encoders (Alley 7.28, and the Resolume codec inside Adobe Media Encoder) on a synthetic pattern (FFmpeg `testsrc2`, plus a synthesized alpha ramp for the alpha variants), 1920 x 1080. Alley files: first 3 frames; AME files: first frame. Packets and sample entry copied byte for byte from the original exports (only the frame count changed). |
| `streams/glenc/` | 20 files: GlEnc's `glenc-cli` 1.0.0 (`-c dxt1/dxt5/ycg6/yg10 --no-audio`) on our own generated pattern (`source/`), sizes 64 x 64 (4 frames), 333 x 221, 18 x 10, 100 x 36 (3 frames), 48 x 48 (1 frame). |
| `streams/synthetic/` | 7 files built by hand in Phase A from the GlEnc block streams of our pattern: legacy `DXDI` (LZF DXT1, stored DXT1, LZF DXT5, the "version nibble 2" implied-DXT1 rule) and DXV3 packets with the stored flag set. Real files of these kinds were not found. |
| `streams/invalid/` | 11 files that a decoder MUST reject (one packet each). |
| `reference/<vector>/` | per packet: the decoded block stream(s) and RGBA, gzip-compressed (`gzip -9n`). |
| `source/` | the PNG frames of our pattern (the input of the GlEnc vectors). |
| `vectors.tsv` | one row per packet: group, frame, packet size, display and coded size, variant, stored flag, expected result (decode / reject), SHA-256 of each UNCOMPRESSED reference output. |

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

RGBA conventions (DXV_FORMAT.md section 8): BC1 / BC3 / BC4 decoded with bit-replicated 565 expansion
and truncating integer interpolation; DXT1 alpha forced to 255 (the variant is opaque; Glance's CPU path
writes alpha 0 for three-colour index 3, which no Resolume stream uses: the forcing is the only
post-processing applied to Glance's output); HQ: YCoCg inverse with nearest-neighbour chroma
(chroma sample (x / 2, y / 2)); no alpha (un)premultiplication. A decoder that matches the block streams
but uses a different BCn rounding will differ from the RGBA by about 1 in some values: compare block
streams first.

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
- Legacy and stored-flag packets: `synthetic` group only.

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
| `bad_dxdi-no-texture-flag` | legacy type byte 0x03 | reject: no texture selected |
| `bad_dxdi-lzf-backref-before-start` | LZF back-reference 8,192 bytes before a 1-byte output | reject |
| `bad_dxdi-lzf-short-output` | LZF stream 64 bytes short of the texture | reject: wrong output length |

Glance's decoder rejected all eleven (for `bad_dxv3-unknown-tag`, by having no decoder for the tag).

## Licence and provenance

All vector files are MIT like the rest of the repository. The pictures are synthetic: our own pattern
(`source/`, generated by an FFmpeg `geq` expression written for this project, see PROVENANCE.md) and
FFmpeg's `testsrc2` test pattern. No camera footage and none of Amrit's clips are in this folder.
