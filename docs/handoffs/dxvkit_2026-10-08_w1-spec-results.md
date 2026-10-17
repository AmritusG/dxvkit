project:    dxvkit
pass:       w1-spec-results
STATUS:     COMPLETE
parent:     NONE (new repo; initial commit)
result:     70e1410fb0d034096ed63ed404f25ce35d2ccdf7
session:    session_01LxGX4yxKucdPVDMKd9cfmG
written_at: 2026-10-08T09:36:00Z

# DXVKit W1 results: Phase A (dirty room) spec + test vectors

## What shipped
- New private repo **AmritusG/dxvkit** (main), `~/Documents/claude/DXVKit`, MIT (Amrit). Brief copied
  to `docs/handoffs/`.
- `docs/spec/DXV_FORMAT.md`: container (FourCC `DXD3` for every DXV3 variant, `DXDI` legacy; sample-entry
  fields as Resolume writes them), the 16x16 padding rule, DXV3 12-byte header, legacy 4-byte header + LZF,
  the DXT1 and DXT5 block-copy layers, the YCG6 / YG10 HQ layer (plane sections, stored / fill /
  FSE-coded opcode streams with the exact table and bitstream rules, the 18 block opcodes in one table,
  the two hashed dictionaries), BC1/BC3/BC4 + YCoCg conversion, a rejection checklist, open questions.
  Prose and tables only.
- `docs/spec/ENCODING_NOTES.md`: what an encoder must emit (incl. the FSE length rule measured below).
- `test_vectors/`: 46 vectors / 102 packets / 11 MB: 8 Resolume (Alley 3 frames + AME 1 frame, each
  variant, testsrc2 1080p, packets byte-identical to the original exports), 20 GlEnc on our own pattern
  (64x64, 333x221, 18x10, 100x36, 48x48 one-frame), 7 synthetic (legacy LZF/stored DXT1, legacy DXT5,
  implied-DXT1 rule, DXV3 stored flag), 11 invalid. Reference block streams + RGBA per packet (gzip),
  `vectors.tsv`, `MANIFEST.sha256`.
- `docs/cleanroom/PROVENANCE.md` (what Phase A read), `INVENTORY.md` (DXV files on this Mac, by folder),
  `CLAUDE.md` with the clean-room HARD RULES and the planned C99 core + C++ header + SwiftPM layout.

## Validated (evidence)
- Reference outputs = Glance's decoder (vendored GlanceCore, glenc-public 8e73f3a) run twice: 218 files
  bit-identical; repo copies equal both runs (0 differ); `vectors.tsv` hashes 218/218; manifest 280/280 OK.
- Spec completeness: a scratch checker written from my format notes (not from upstream code, not
  committed) reproduced all 127 valid block streams byte for byte and rejected all 11 invalid vectors.
- Glance rejected all 11 invalid vectors.

## Findings that change what we believed
1. **DXV3 pads BOTH width and height to 16** (brief said width only). Resolume 908x2276 → 912x2288,
   1920x1080 → 1920x1088, 1920x818 → 1920x832: the only sizes whose decode consumes the packet exactly.
   Legacy DXDI too (1920x1080 LZF output = 1,044,480 bytes = 1920x1088).
2. Glance's "+1 opcode tolerance for Alley" is an artefact of decoding with 4-row height alignment;
   with 16x16 every Resolume stream uses every opcode and every literal exactly.
3. Real Resolume HQ files use the **FSE-coded** opcode streams and **all 18 opcodes**; GlEnc uses only
   stored/fill and opcodes 0, 1, 3. Only the `resolume` vectors cover FSE. Alley's bundle ships the FSE
   (Yann Collet, BSD-2) licence: the spec cites public FSE docs / RFC 8878.
4. Every Resolume FSE stream leaves exactly 32 bits unread: bit length = 32 + bits consumed.
5. **Alpha:** Alley writes straight colour (97.6 % of semi-transparent pixels have colour > alpha); the
   Resolume codec in Adobe Media Encoder wrote premultiplied-looking colour (0.1 %). The file does not say.
6. Resolume uses BC1 three-colour blocks but never index 3 (38,115 blocks checked) → DXT1 is safely opaque.
7. Glance's CPU BC1 path differs from FFmpeg's by ±1 (bit replication vs rounding) and writes alpha 0 for
   index 3; reference RGBA uses Glance's values with DXT1 alpha forced to 255 (documented).

## Variants / files
- Found: DXT1, DXT5, YCG6, YG10 (all `DXD3`); legacy DXT1 (`DXDI`, type byte 0x23) in one clip
  (`~/Movies/Testfiles/Metric_01.mov`). 695 DXV files, 397,323 packets: every DXV3 header version 4 /
  minor 0 / stored 0 / reserved 0 / exact length. No file mixes variants.
- Seen nowhere (open unknowns): legacy DXT5 / stored legacy, DXV3 stored flag, HQ opcode >= 18, other
  version bytes, other sample-entry FourCCs. Synthetic vectors cover the decodable ones.
- Writers on this Mac: Alley (GUI only), glenc-cli, FFmpeg (DXT1 only); nothing writes DXDI.

## Deviations
- Vectors from Resolume itself came from GlEnc's reference corpus (Alley / AME exports of FFmpeg's
  synthetic testsrc2), trimmed byte-exactly, instead of new Alley exports (Alley has no CLI). Content is
  synthetic, not footage.
- Legacy and stored-flag vectors are hand-built (no encoder exists); invalid vectors hand-corrupted.
- Reference file suffix renamed `cocg` → `chroma` (the self-check flagged it as an upstream name).

## Needs Amrit
- WHAT: decide straight vs premultiplied for DXVKit's alpha output. HOW: drop one Alley and one AME DXT5
  alpha clip of the same source on a layer over a background in Arena; tell us if they look the same.
  WHERE: Arena, any comp. (Until then the decoder outputs the decoded values untouched.)
- Optional: an Alley export of `test_vectors/source/pat333x221.*` (all four DXV variants) into
  `~/Movies/Testfiles/dxvkit-alley/` would add Resolume-encoded odd-size vectors.
- Cowork: write the Phase B brief (clean room reads only DXVKit spec/vectors/cleanroom + public docs).
- No questions were asked of Amrit in this session.

## RECEIPTS
```
$ git log -1 --format='%H %an %ad%n%s'
70e1410fb0d034096ed63ed404f25ce35d2ccdf7 Thu Oct 8 11:33:11 2026 +0200
DXVKit W1 (Phase A, dirty room): DXV format spec, encoding notes, test vectors
$ git status -sb | head -1
## main...origin/main
$ git ls-remote origin main
70e1410fb0d034096ed63ed404f25ce35d2ccdf7	refs/heads/main
# Glance reproducibility (two full runs) + repo copies
RUN1==RUN2 (     218 files,      102 frame lines)
repo reference == Glance run1 == run2: 218 files, 0 differ
vectors.tsv hashes verified: 218 bad: 0
$ shasum -a 256 -c MANIFEST.sha256 | grep -vc ': OK$'    (280 entries)
0
# scratch checker from the spec notes vs the repo vectors
ok streams 127 problems 0
# self-check on docs/spec (upstream identifiers: dxv_ op_ AV_ avctx bytestream CHECKPOINT cgo cocg yo tab0-3 val1/2 lshift rshift old_type version_major ff_ AVERROR size_in_bits + ~60 FFmpeg/Glance function names)
identifier hits in docs/spec: 0      code fences in docs/spec: 0      line refs: 0
$ du -sh test_vectors test_vectors/streams test_vectors/reference test_vectors/source
 11M test_vectors   3.2M streams   7.5M reference   68K source
$ ls -la test_vectors/streams/resolume (bytes)
282951 resolume-alley_dxt1_testsrc2_1920x1080.mov   560978 resolume-alley_dxt5_testsrc2_1920x1080.mov
357581 resolume-alley_ycg6_testsrc2_1920x1080.mov   595223 resolume-alley_yg10_testsrc2_1920x1080.mov
 94194 resolume-ame_dxt1_testsrc2_1920x1080.mov     225172 resolume-ame_dxt5_testsrc2_1920x1080.mov
117414 resolume-ame_ycg6_testsrc2_1920x1080.mov     305826 resolume-ame_yg10_testsrc2_1920x1080.mov
$ ls test_vectors/streams/{glenc,synthetic,invalid} | wc -l  ->  20 / 7 / 11 files (sizes 606 B .. 190,902 B)
# header scan, all DXV files on this Mac (condensed onto one line)
695 files 397323 packets
161827 DXD3 YCG6 v4.0 raw0 b7=0 sizeok | 122048 DXT5 | 110051 DXT1 | 3191 YG10 | 206 DXDI type=0x23 sizeok
```
Wire / Arena not touched.
END OF HANDOFF w1-spec-results sha256:fefb81cc 7007
