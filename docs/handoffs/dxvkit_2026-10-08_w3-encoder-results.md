project:    dxvkit
pass:       w3-encoder-results
STATUS:     COMPLETE
parent:     0e1a9685fc23d5b8d5f8a33a021c7e108456b574
result:     11baeca7b77da51bc32a6e34b65198095ee2fdaf
session:    a3fd4145-83a7-4e1e-931f-72534cdaed15
written_at: 2026-10-08T14:30:15Z

# DXVKit W3 results: clean-room DXV3 encoder

## Amrit's questions
- "Clean room. Can it be made to work / merge / replace Glance and the others you built?" For **encoding,
  yes**. DXVKit now encodes all four DXV3 variants (DXT1, DXT5, YCG6, YG10) from RGBA8, or packs a caller's
  own BCn planes, and writes the .mov. Resolume Arena opens and decodes every output. Our decoder gets back
  exactly the packed block streams, with every byte consumed. On Alley's own blocks our packer is the same
  size or smaller than Alley's. GlEnc can drop its LGPL writers for it (swap notes below; a later GlEnc job).
- "Since the encoder is separate, can it run now while I update Glance?" It did. Glance pins cdbb4bc. This wave
  touched no decoder source file (`git diff 0e1a968` on the decoder files: empty).

## What shipped (commit 11baeca, pushed)
- **C99 encoder** (libc only, no globals, allocation only in `dxvk_encoder_create`, caller packet buffer,
  `dxvk_jobs` for BCn row bands + the two HQ sections; output identical with or without jobs):
  BC1 / BC3 / BC4 block encoders (own, from the public BCn rules, error measured on our decoder's palette);
  DXT1 / DXT5 block-copy packers; HQ opcode chooser mirroring the decoder's dictionaries (all 18 opcodes);
  FSE (tANS, table log 10, spread 641) / stored / fill opcode streams, cheapest per stream; forward YCoCg
  (Y = (R+2G+B+2)>>2, Co/Cg = rounded 2x2 means + 128); padding to 16 x 16 by edge replication; DXV3 header
  4/0/0/0; optional stored-flag packets (DXT1 / DXT5 only, off by default).
- **API** (`include/dxvkit.h`): `dxvk_encode_bound`, `dxvk_encoder_create/destroy/layout`,
  `dxvk_encoder_compress` (RGBA -> planes), `dxvk_encoder_pack` (blocks-in path), `dxvk_encoder_encode`
  (both), `dxvk_encoder_plane`; `dxvk_encode_options {flags, effort FAST|BEST, opcodes AUTO|STORED, jobs}`,
  flags `DXVK_ENC_PREMULTIPLIED_INPUT` (declarative: bytes unchanged), `DXVK_ENC_STORED_PAYLOAD`.
  **MOV writer**: `dxvk_mov_writer_open_file/open_sink/add/finish/close`: ftyp qt, wide, mdat (64-bit when
  > 4 GB), moov with the 86-byte DXD3 entry exactly as spec 2.2 (FFMP, 512/512, 72 dpi, depth 24), stts one
  entry, stsc one sample per chunk, stsz, stco / co64.
- **Effort**: FAST = principal-axis BC1 + one refit, min/max BC4, 2^16 match tables, one opcode pass with a
  fixed cost prior. BEST = iterated refits + +-1 endpoint search + three-colour mode (DXT1), BC4 endpoint
  search in both modes, up to 2^20 match tables, DXT5 alpha runs cut before long block runs, two opcode
  passes with measured costs.
- **C++** `dxvk::Encoder`, `dxvk::MovWriter`; **Swift** `DXVEncoder`, `DXVEncodeOptions`, `DXVMovieWriter`
  (+ `DXVEffort`, `DXVOpcodeCoding`).
- **dxvtool**: `encode IN... OUT.mov --variant --effort [--size --fps --threads --opcodes --premultiplied
  --stored-payload]` (PNG / PAM / PPM via system zlib, or raw RGBA / stdin), `bench-encode`, `repack`, `psnr`,
  `cmp-rgba [--unpremultiply-b]`, `dump-samples`, `check --histogram`.
- Tests / tools: `tests/encoder_test.c` (ctest), `tools/fuzz_encode.c`, `tools/oracle.sh`,
  `tools/gen_bc_tables.c` -> `src/enc_tables.h`. Docs: `docs/design-encoder.md` (spec extract + design),
  QUESTIONS 10-20, PROVENANCE W3 attestation, README.

## Round trip (encode -> our decoder): 112 / 112 identical, every packet consumed exactly
Variants x {fast, best} x {HQ: auto / stored opcodes; DXT1/DXT5: compressed / stored flag} x sizes 16x16,
18x10, 333x221, 1920x1080, 908x2276, 3840x2160, 1x1. Each case: planes identical after decode, payload /
literals / opcodes unused = 0, every FSE stream leaves 32 bits, one-step = compress+pack = threaded bytes.
Packet bytes for our synthetic test picture (fast / best, compressed):

| size | DXT1 | DXT5 | YCG6 | YG10 |
|---|---|---|---|---|
| 16x16 | 152 / 152 | 214 / 214 | 240 / 240 | 300 / 300 |
| 18x10 | 185 / 185 | 301 / 309 | 350 / 347 | 469 / 474 |
| 333x221 | 18,195 / 18,180 | 24,703 / 24,660 | 22,647 / 22,655 | 28,050 / 28,064 |
| 1920x1080 | 345,134 / 359,349 | 491,125 / 502,780 | 472,116 / 466,834 | 599,243 / 593,961 |
| 908x2276 | 363,377 / 371,950 | 513,836 / 520,059 | 482,472 / 476,891 | 615,331 / 609,742 |
| 3840x2160 | 1,224,883 / 1,271,531 | 1,577,540 / 1,612,592 | 1,808,515 / 1,784,059 | 2,083,232 / 2,058,776 |
| 1x1 | 24 / 24 | 33 / 33 | 63 / 63 | 78 / 78 |
(The test picture is one-sixth random noise on purpose, so these are not typical sizes.)
Also: `dxvtool check` on all Arena files: 960 packets, 0 errors, 0 inexact, 0 dangling tails.

## Black-box oracle (ffmpeg 8.1.1 binary): pass where ffmpeg is a valid reference
`tools/oracle.sh`, testsrc2 + alpha ramp, 2 frames, every variant and effort:
- **16x16, 333x221, 1920x1080, 3840x2160**: decoded without a single warning. **YCG6 / YG10: ffmpeg's native
  yuv(a)420p planes identical to our sample planes** (ffmpeg U = Cg, V = Co). **DXT1: max |d| = 1** (0.6 % of
  bytes at 1080p; BC1 rounding convention: ours = spec's bit replication + truncating thirds; ffmpeg's is
  different and has no closed form among 12 tried, the nearest being multiply-round expansion at 98.9 %).
  **DXT5: alpha identical; colour max |d| = 2** at 1080p / 4K (4 at 16x16) after applying ffmpeg's
  **un-premultiply** (ffmpeg outputs floor(c x 255 / a): it treats DXT5 as premultiplied, QUESTIONS 11).
  Opaque-source DXT5: max |d| = 1.
- **18x10, 908x2276, 1x1: ffmpeg is not a reference.** It sizes the texture with the width rounded to 4, not
  16. Proven: our 908-wide stream re-indexed at 227 blocks/row matches ffmpeg within +-1 on every pixel, and
  ffmpeg mis-decodes **Resolume's own 908x2276 DXT1 file** the same way (5.77 M of 8.27 M bytes wrong, no
  warning). 18x10 HQ / 1x1: ffmpeg rejects "Invalid data". Arena decodes all these sizes correctly.
- Encoder fix found by the oracle: ffmpeg reads BC3 colour with c0 <= c1 as three-colour; the encoder now
  always orders BC3 colour endpoints c0 > c1 (QUESTIONS 10).

## Resolume Arena 7.28 (MCP; open comp "LoadEhr", **not saved**, nothing triggered)
New **layer 6 "DXVKit Encoder"** (top, opacity 0.50 default: set 1.0 before firing). Every clip reports the
right type and size, and its regenerated thumbnail (decoded by Arena) is correct:

| col | file (out/w3/) | Arena reports |
|---|---|---|
| 1 | dxvkit-w3_dxt1_testsrc2_1920x1080.mov | DXV 3.0 Normal Quality, No Alpha, 1920x1080, 30 fps, 5 s |
| 2 | dxvkit-w3_dxt5_testsrc2_1920x1080.mov | Normal Quality, With Alpha (alpha ramp visible) |
| 3 | dxvkit-w3_ycg6_testsrc2_1920x1080.mov | High Quality, No Alpha |
| 4 | dxvkit-w3_yg10_testsrc2_1920x1080.mov | High Quality, With Alpha |
| 5 | dxvkit-w3_yg10_testsrc2_333x221.mov | HQ With Alpha, 333x221 |
| 6 | dxvkit-w3_dxt1_testsrc2_908x2276.mov | Normal Quality, 908x2276 (no shearing) |
| 7 | dxvkit-w3_ycg6_testsrc2_908x2276.mov | High Quality, 908x2276 |
| 8 | dxvkit-w3_dxt5_testsrc2_18x10.mov | Normal Quality With Alpha, 18x10 |
| 9 | dxvkit-w3_dxt1-storedflag_testsrc2_333x221.mov | stored-flag packets: thumbnail correct |
| 10 | dxvkit-w3_dxt5-storedflag_testsrc2_333x221.mov | stored-flag packets: thumbnail correct |
Alley: GUI only, cannot be driven without its GUI: not used.

## vs Alley (testsrc2 1080p, frames 0-2; AME = frame 0)
**Packer on identical blocks (fair):** `dxvtool repack` decodes Resolume's packets and re-packs the same
blocks: DXT1 +0.05 % (fast) / -0.01 % (best), DXT5 -1.6 % / **-9.0 %**, YCG6 -0.4 % / -0.5 %, YG10 -0.3 % /
-0.4 % (Alley); AME: DXT1 +0.06 / +0.01 %, DXT5 +0.1 / -6.2 %, YCG6 -0.4 / -0.5 %, YG10 +0.4 / -1.4 %.
Blocks identical after repack in all 16 runs.
**Full encode** (the true source is not in the repo; reconstructed by experiment, QUESTIONS 20; it matches
Alley's decode at only ~33-34 dB, so **PSNR favours us and does not rank the encoders**):

| variant | encoder | mean packet B | PSNR RGB vs reconstruction | alpha |
|---|---|---|---|---|
| DXT1 | Alley / AME | 94,112 / 93,595 | 33.13 / 33.18 dB | - |
| DXT1 | DXVKit fast / best | 99,080 / 100,027 | 39.70 / 39.88 dB | - |
| DXT5 | Alley / AME | 186,788 / 224,573 | 31.10 / 13.45 dB (AME premultiplied) | exact / exact |
| DXT5 | DXVKit fast / best | 190,352 / 177,876 | 39.70 / 39.86 dB | exact |
| YCG6 | Alley / AME | 118,989 / 116,815 | 34.27 / 34.24 dB | - |
| YCG6 | DXVKit fast / best / fast+stored | 123,400 / 123,218 / 141,742 | 37.95 / 38.03 dB | - |
| YG10 | Alley / AME | 198,203 / 305,227 | 32.41 / 13.43 dB | exact / exact |
| YG10 | DXVKit fast / best / fast+stored | 207,406 / 207,224 / 267,176 | 37.95 / 38.03 dB | exact |
Sizes: ours are +4..+6 % (DXT1, YCG6, YG10) and -5 % (DXT5 best) versus Alley. Since the packer is at parity,
the difference comes from our BCn blocks on a softer reconstructed source (blurrier chroma: our chroma
literal bytes run about 8 % above Alley's, luma literal bytes are equal). A fair quality table needs Alley to encode
our exact source (below).

## Speed (MacBook Pro M3 Max 16-core, Release, encode = compress + pack, ms/frame; testsrc2 + alpha)
Running at the same time: Arena (~80 % CPU, playing a LoadEhr clip), Spotlight indexing (~120 %), Time
Machine; load average 7-13. No busy process from another Claude session showed in `ps`.

| variant | 1080p fast 1 thr / 16 | 1080p best 1 / 16 | 4K fast 1 / 16 | 4K best 1 / 16 |
|---|---|---|---|---|
| DXT1 | 8.3 / 1.1 (121 / 912 fps) | 24.2 / 2.9 | 30.5 / 4.0 (33 / 250 fps) | 91.7 / 11.4 |
| DXT5 | 10.2 / 1.4 | 15.9 / 2.1 | 37.0 / 5.0 | 58.3 / 7.6 |
| YCG6 | 5.5 / 1.1 (181 / 897 fps) | 12.9 / 2.5 | 20.9 / 4.2 | 55.0 / 10.0 |
| YG10 | 8.4 / 2.0 | 16.3 / 3.9 | 30.1 / 7.1 (33 / 142 fps) | 66.2 / 14.6 |
With jobs, every variant and effort encodes 4K above 60 fps. HQ scales less (section 1 is a single
sequential job). Real-time fast single-thread: 1080p yes for all; 4K only YCG6 (48 fps).

## Robustness
ASan + UBSan: all 112 round-trip cases, vectors (single + threaded), C++ test: 0 failures, 0 reports.
libFuzzer `fuzz_encode` (random size 1..320, pixels, all options, RGBA and blocks-in paths, every output
decoded and checked exact): 30 min x 6 workers, **793,937 execs, 0 crashes / reports / artifacts**.
Decoder unchanged: verify **127/127, 91/91, 11/11**.

## Deviations
- Padding by edge replication (brief) rather than the notes' flat black.
- `DXVK_ENC_PREMULTIPLIED_INPUT` changes no bytes (the file has no field for the convention), see QUESTIONS 16.
- ffmpeg oracle: sizes with ceil4(W) != ceil16(W) are reported as "ffmpeg geometry", not failures (proven
  ffmpeg limitation, also on Resolume's own file).
- Alley PSNR comparison uses a reconstructed source (biased); the packer comparison is exact.
- Added beyond the brief: `repack`, `psnr`, `cmp-rgba`, `dump-samples`, `check --histogram`, PNG input.
- dxvtool links the system zlib (zlib licence) for PNG input; the library stays libc only.

## QUESTIONS (W3 items 10-20; full text in docs/cleanroom/QUESTIONS.md)
10 BC3 colour: ffmpeg applies the BC1 order rule -> encoder orders c0 > c1 (settled; Phase A: add to spec 8.2).
11 ffmpeg un-premultiplies DXT5 (measured; feeds the straight / premultiplied question).
12 ffmpeg uses 4-aligned width (proven; our files follow Resolume's 16 x 16, Arena confirms).
13 ffmpeg BC1 rounding differs by +-1 (characterised).
14 Stored-flag packets: Arena opens + thumbnails them; playback unverified (Amrit: columns 9 / 10).
15 Edge-replication padding (brief). 16 Premultiplied flag is declarative. 17 Single-symbol FSE impossible ->
fill. 18 No dangling opcode written. 19 Default opcodes = auto (FSE); stored +15 % (YCG6) / +29 % (YG10).
20 Alley source reconstructed (approximate).
Still open from W2 for Phase A: selector 3; the dangling-opcode wording (W2 items 1, 3).

## Attestation (full text in PROVENANCE.md)
Fresh session a3fd4145-83a7-4e1e-931f-72534cdaed15 after /clear (context held no FFmpeg / Glance / GlEnc source).
Read only this repo (spec, notes, design, cleanroom docs, handoffs, vectors, DXVKit's own W2 code) plus
public BCn / FSE / QuickTime / PNG knowledge; no web fetch or search. Black box only: ffmpeg / ffprobe
binaries (decode + compare, synthetic sources) and Arena (load, thumbnails). **Did not access** FFmpeg source,
Glance, glenc-public, GlEnc (including its reference folder), CueGlance, MaCrate, Alley internals or any DXV
implementation. No accidental exposure. Nothing vendored.

## What GlEnc needs to swap its LGPL DXV writers (DXVLZWriter, DXVHQOpcodeWriter, DXVHQCgoWriter)
- Link the SwiftPM product `DXVKit` (pin 11baeca or later). Per output file: `DXVEncoder(variant:width:height:
  options:)` once, then per frame `encode(rgba:bytesPerRow:)` -> packet `Data` (allocation-free inside).
- Inputs GlEnc must provide: RGBA8, display size, rows top first, **straight alpha** (what Alley writes; set
  `premultipliedInput` if GlEnc's frames are premultiplied, which records the convention only). The display size
  is used as is; DXVKit pads to 16 x 16 itself (GlEnc must not pre-pad). No YCoCg conversion on GlEnc's side.
  For HQ, DXVKit does the transform and chroma averaging.
- Option mapping: GlEnc codec dxt1 / dxt5 / ycg6 / yg10 -> `DXVVariant`; a quality / speed setting ->
  `effort .fast` (live, batch) / `.best` (archive); keep `opcodes .auto` (what Resolume writes) unless
  GlEnc needs byte-compatibility with its old stored / fill streams (`.stored`); `parallel: true` on multicore.
- If GlEnc keeps its own BCn encoder: `pack(plane0:plane1:)` with planes in `encoder.layout` (BC1 | BC3 |
  BC4 Y + BC5 CoCg | BC5 YA + BC5 CoCg, padded to 16 x 16).
- Container: either `DXVMovieWriter` (video-only .mov; no audio), or keep GlEnc's own muxer (AVAssetWriter
  passthrough etc.) for audio and copy only DXVKit's sample-entry values (DXD3, vendor FFMP, quality 512,
  depth 24, entry size 86).
- Check after the swap: `dxvtool check` on GlEnc output (expect 0 inexact), a few files in Arena.

## Needs Amrit (WHAT / HOW / WHERE)
1. **Fire the encoder clips** to confirm live playback (thumbnails are Arena-decoded and correct already).
   HOW: set layer 6 opacity to 1.0, trigger columns 1-10 one by one; columns 9 / 10 settle whether Resolume
   plays stored-flag packets. WHERE: Arena, open comp "LoadEhr", layer 6 "DXVKit Encoder". The comp is
   unsaved; DXVKit did not save it (save or discard as you like).
2. Optional, gives the fair quality table: **export our exact source through Alley**. HOW: in Alley, open
   `~/Documents/claude/DXVKit/out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle.mov` (lossless RGBA,
   3 frames) and export all four DXV variants. WHERE: save as `out/w3/alley_<variant>.mov` and tell a DXVKit
   session; it then runs `dxvtool psnr` against the same RGBA for both encoders.
3. Still open: straight vs premultiplied in Arena. New data point: ffmpeg treats DXT5 as premultiplied
   (QUESTIONS 11).
- Amrit's answers from this session: none (no questions were asked).

## RECEIPTS
```
$ git log -1 --format="%H %an %ad%n%s"; git status -sb | head -1; git ls-remote origin main
11baeca7b77da51bc32a6e34b65198095ee2fdaf Amrit Rosell Thu Oct 8 16:27:09 2026 +0200
DXVKit W3 (Phase B, clean room): DXV3 encoder for DXT1, DXT5, YCG6, YG10, QuickTime writer
## main...origin/main
11baeca7b77da51bc32a6e34b65198095ee2fdaf	refs/heads/main
$ (cd build && ctest)   # Release, universal
1/4 Test #1: vectors ..........................   Passed    0.53 sec
2/4 Test #2: vectors_threaded .................   Passed    0.51 sec
3/4 Test #3: cpp_wrapper ......................   Passed    0.08 sec
4/4 Test #4: encoder_round_trip ...............   Passed   17.28 sec
100% tests passed, 0 tests failed out of 4
$ build/dxvtool verify test_vectors/vectors.tsv | tail -1
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256 | grep -vc ": OK$")
0
$ git diff 0e1a968 --stat -- src/packet.c src/legacy.c src/blockcopy.c src/hq.c src/pixels.c src/mov.c src/dxvk_priv.h   (decoder sources)
       0
$ swift test | grep Executed
	 Executed 3 tests, with 0 failures (0 unexpected) in 0.724 (0.725) seconds
$ ASan+UBSan (build-asan, Debug, -fno-sanitize-recover=all): dxvkit_encoder_test (all 7 sizes), verify, verify --threads 4, cpp test
exit 0
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
cpp encoder + mov writer: ok (packet 193075 bytes)
cpp wrapper: 25 frames, 0 failures
sanitizer report lines: 0
$ libFuzzer fuzz_encode (Homebrew LLVM, ASan+UBSan), 6 workers x 1800 s: start / end UTC, per-worker DONE
2026-10-08T13:46:13Z
2026-10-08T14:16:15Z
#132330	DONE   cov: 958 ft: 5595 corp: 959/404Kb lim: 4102 exec/s: 73 rs
#131855	DONE   cov: 958 ft: 5595 corp: 959/404Kb lim: 4102 exec/s: 73 rs
#133033	DONE   cov: 958 ft: 5595 corp: 950/397Kb lim: 4102 exec/s: 73 rs
#133911	DONE   cov: 958 ft: 5595 corp: 952/386Kb lim: 4102 exec/s: 74 rs
#130804	DONE   cov: 958 ft: 5595 corp: 952/410Kb lim: 4102 exec/s: 72 rs
#132004	DONE   cov: 958 ft: 5595 corp: 958/399Kb lim: 4102 exec/s: 73 rs
crash/leak/timeout/oom artifacts: 0  error lines: 0
$ tools/oracle.sh build/dxvtool WORK (all 7 sizes) | tail -1
ORACLE: all files decoded by ffmpeg without warnings and matched
$ build/dxvtool check out/w3/*.mov | tail -1
out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle.mov: no DXV video track
files 11 (bad 1) | packets 960 | errors 0 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 0
$ ls -la out/w3   (git-ignored; Arena layer 6 files)
drwxr-xr-x  13 amritrosell  staff       416 Oct  8 16:02 .
drwxr-xr-x   3 amritrosell  staff        96 Oct  8 15:45 ..
-rw-r--r--   1 amritrosell  staff   1938703 Oct  8 16:02 dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle.mov
-rw-r--r--   1 amritrosell  staff   2259740 Oct  8 15:50 dxvkit-w3_dxt1-storedflag_testsrc2_333x221.mov
-rw-r--r--   1 amritrosell  staff  11191828 Oct  8 15:45 dxvkit-w3_dxt1_testsrc2_1920x1080.mov
-rw-r--r--   1 amritrosell  staff   4375040 Oct  8 15:47 dxvkit-w3_dxt1_testsrc2_908x2276.mov
-rw-r--r--   1 amritrosell  staff   4517660 Oct  8 15:50 dxvkit-w3_dxt5-storedflag_testsrc2_333x221.mov
-rw-r--r--   1 amritrosell  staff     14246 Oct  8 15:47 dxvkit-w3_dxt5_testsrc2_18x10.mov
-rw-r--r--   1 amritrosell  staff  22739743 Oct  8 15:45 dxvkit-w3_dxt5_testsrc2_1920x1080.mov
-rw-r--r--   1 amritrosell  staff  12277662 Oct  8 15:45 dxvkit-w3_ycg6_testsrc2_1920x1080.mov
-rw-r--r--   1 amritrosell  staff   6272049 Oct  8 15:47 dxvkit-w3_ycg6_testsrc2_908x2276.mov
-rw-r--r--   1 amritrosell  staff  24878562 Oct  8 15:45 dxvkit-w3_yg10_testsrc2_1920x1080.mov
-rw-r--r--   1 amritrosell  staff    898131 Oct  8 15:47 dxvkit-w3_yg10_testsrc2_333x221.mov
Arena: composition "LoadEhr" (Arena 7.28), NOT saved; layer 6 "DXVKit Encoder" added, 10 clips (clip find "dxvkit-w3" -> 10), none triggered; diagnose: no issues at warning level.
(check: the one "bad" file is the qtrle Alley-source file, which has no DXV track; all 10 DXV files: 960 packets, 0 errors, 0 inexact.)
$ build/dxvtool bench-encode (speed table source, 32 lines)
DXT1 1920x1080 effort fast threads 1 frames 30: 8.28 ms/frame, 120.7 fps, mean packet 74886 B
DXT1 1920x1080 effort fast threads 16 frames 30: 1.10 ms/frame, 911.7 fps, mean packet 74886 B
DXT1 1920x1080 effort best threads 1 frames 30: 24.16 ms/frame, 41.4 fps, mean packet 74322 B
DXT1 1920x1080 effort best threads 16 frames 30: 2.88 ms/frame, 347.8 fps, mean packet 74322 B
DXT5 1920x1080 effort fast threads 1 frames 30: 10.15 ms/frame, 98.6 fps, mean packet 161390 B
DXT5 1920x1080 effort fast threads 16 frames 30: 1.41 ms/frame, 707.9 fps, mean packet 161390 B
DXT5 1920x1080 effort best threads 1 frames 30: 15.86 ms/frame, 63.1 fps, mean packet 151178 B
DXT5 1920x1080 effort best threads 16 frames 30: 2.07 ms/frame, 482.0 fps, mean packet 151178 B
YCG6 1920x1080 effort fast threads 1 frames 30: 5.52 ms/frame, 181.1 fps, mean packet 80446 B
YCG6 1920x1080 effort fast threads 16 frames 30: 1.12 ms/frame, 896.8 fps, mean packet 80446 B
YCG6 1920x1080 effort best threads 1 frames 30: 12.85 ms/frame, 77.8 fps, mean packet 80857 B
YCG6 1920x1080 effort best threads 16 frames 30: 2.45 ms/frame, 408.4 fps, mean packet 80857 B
YG10 1920x1080 effort fast threads 1 frames 30: 8.39 ms/frame, 119.2 fps, mean packet 164452 B
YG10 1920x1080 effort fast threads 16 frames 30: 2.04 ms/frame, 491.4 fps, mean packet 164452 B
YG10 1920x1080 effort best threads 1 frames 30: 16.34 ms/frame, 61.2 fps, mean packet 164863 B
YG10 1920x1080 effort best threads 16 frames 30: 3.88 ms/frame, 257.7 fps, mean packet 164863 B
DXT1 3840x2160 effort fast threads 1 frames 6: 30.53 ms/frame, 32.8 fps, mean packet 261907 B
DXT1 3840x2160 effort fast threads 16 frames 6: 4.00 ms/frame, 249.7 fps, mean packet 261907 B
DXT1 3840x2160 effort best threads 1 frames 6: 91.74 ms/frame, 10.9 fps, mean packet 258288 B
DXT1 3840x2160 effort best threads 16 frames 6: 11.42 ms/frame, 87.6 fps, mean packet 258288 B
DXT5 3840x2160 effort fast threads 1 frames 6: 36.97 ms/frame, 27.0 fps, mean packet 441653 B
DXT5 3840x2160 effort fast threads 16 frames 6: 4.99 ms/frame, 200.3 fps, mean packet 441653 B
DXT5 3840x2160 effort best threads 1 frames 6: 58.27 ms/frame, 17.2 fps, mean packet 397165 B
DXT5 3840x2160 effort best threads 16 frames 6: 7.63 ms/frame, 131.0 fps, mean packet 397165 B
YCG6 3840x2160 effort fast threads 1 frames 6: 20.88 ms/frame, 47.9 fps, mean packet 284820 B
YCG6 3840x2160 effort fast threads 16 frames 6: 4.18 ms/frame, 239.4 fps, mean packet 284820 B
YCG6 3840x2160 effort best threads 1 frames 6: 55.03 ms/frame, 18.2 fps, mean packet 286814 B
YCG6 3840x2160 effort best threads 16 frames 6: 9.96 ms/frame, 100.4 fps, mean packet 286814 B
YG10 3840x2160 effort fast threads 1 frames 6: 30.06 ms/frame, 33.3 fps, mean packet 470171 B
YG10 3840x2160 effort fast threads 16 frames 6: 7.05 ms/frame, 141.8 fps, mean packet 470171 B
YG10 3840x2160 effort best threads 1 frames 6: 66.15 ms/frame, 15.1 fps, mean packet 472164 B
YG10 3840x2160 effort best threads 16 frames 6: 14.59 ms/frame, 68.5 fps, mean packet 472164 B
```
END OF HANDOFF w3-encoder-results sha256:43c16640 22194
