project:    dxvkit
pass:       w2-cleanroom-results
STATUS:     COMPLETE
parent:     688c184328aaada2bb6e38754df4b2813f2f123b
result:     cdbb4bc0f2407d7c3c88d4b050592e0e1fec2da7
session:    a1c3b38d-05d5-4958-b2d4-ab4bb740f08e
written_at: 2026-10-08T11:04:10Z

# DXVKit W2 results: Phase B (clean room) decoder

## Amrit's question: "Can it be made to work / merge / replace Glance and the others you built?"
For **decoding, yes**. DXVKit's output matches the reference (Glance's decoder, produced in Phase A) byte
for byte on every vector, block streams and RGBA. It also decodes all 320,888 packets of the 426 DXV files in
~/Movies; the only error is a deliberately corrupted fixture. The Swift API (`DXVMovie` / `DXVFrame`)
has the shape Glance-style callers use (open, frame count / size / variant, frame N to BCn buffers or
RGBA). Each app swap is still its own job, as the brief says. GlEnc additionally needs the encoder (a later wave).

## What shipped (commit cdbb4bc, pushed)
- **C99 core** (`src/`, libc only, no globals, no allocation or threads in the decode path, MIT headers):
  MOV demux (DXD3 / DXDI, stsz, stsc runs, stco + co64, every sample bounds-checked at open; file,
  memory or caller reader), packet decode for legacy LZF / stored, DXV3 DXT1, DXT5, YCG6, YG10 (stored /
  fill / FSE opcode streams, 18 opcodes, dictionaries) and the stored flag, and RGBA8 with an optional
  straight-to-premultiplied conversion. The checklist in spec 9 maps to 24 status codes. A per-decode
  consumption report shows payload, literal and opcode slack and the FSE bits left.
- **API** (`include/dxvkit.h`): `dxvk_mov_open_file/_memory/_reader`, `dxvk_mov_get_info`,
  `dxvk_mov_read_packet`, `dxvk_packet_variant`, `dxvk_layout`, `dxvk_decode_blocks` (planes: BC1 | BC3 |
  BC4 Y + BC5 CoCg | BC5 YA + BC5 CoCg, whole 16-padded texture), `dxvk_blocks_to_rgba` (display size,
  caller stride), `dxvk_jobs` (caller "run N jobs" callback, used for the 2 HQ sections + 32 RGBA bands).
  Alpha is untouched by default; `DXVK_OPT_PREMULTIPLY` is opt-in. `dxvk_variant_has_alpha` exposes alpha presence.
- **C++** `include/dxvkit.hpp`: header-only `dxvk::Movie` / `dxvk::Frame`, RAII, no exceptions.
- **SwiftPM** (`Package.swift`, module `DXVKit`): `DXVMovie` (thread-safe), `DXVFrame` (reusable buffers,
  `withPlane`, `rgba`), `DXVLayout`, `DXVOptions(premultiply:, parallel:)` (parallel uses Dispatch).
- **dxvtool**: info / dump-blocks / dump-rgba / verify / check / bench. Fuzz harnesses + corpus tool.
- **Docs**: `docs/design.md`, `docs/cleanroom/QUESTIONS.md`, Phase B attestation in PROVENANCE.md, README.
- Universal arm64 + x86_64 with CMake and with SwiftPM. `-Wall -Wextra -Werror -Wshadow -Wconversion` on the core.

## Validated
| Gate | Result |
|---|---|
| Vectors, block streams | **127 / 127 identical** (resolume 16 rows, glenc 56, synthetic 19) |
| Vectors, RGBA | **91 / 91 identical** |
| Invalid vectors | **11 / 11 rejected**, each with the matching reason (LZF, length, no texture, distance x2, header length, payload end, unknown tag, too short, opcode count, section offset) |
| Threaded (jobs) | identical results (verify `--threads`, C++ test, Swift parallel) |
| ctest / swift test | 3/3 / 2/2 pass |
| ASan + UBSan | all vectors, single and threaded, ctest: 0 reports |
| libFuzzer (Homebrew LLVM 22, ASan+UBSan) | packet decode 30 min x 6 workers, **7,894,004 execs**; MOV demux 30 min x 6 workers, **244,394,598 execs**; **0 crashes, 0 sanitizer reports, 0 timeouts / OOMs**; seed corpus = the vectors |
| Real files, brief's 20 (paths below) | **45,262 packets, 0 errors**. Every packet is consumed exactly, or exactly apart from the spec 7.5 dangling-opcode tail (13,408 YCG6 packets) |
| Real files, bonus: all 426 DXV files in ~/Movies (257 GB) | **320,888 packets** (DXT1 83,678 / DXT5 72,171 / YCG6 161,737 / YG10 3,101 / legacy 201). **1 error**: `Testfiles/bad-audio-fixtures/bad-audio.mov` frame 115, FSE table rejected. That file differs from its twin `good-audio.mov` in exactly one 4,000-byte run inside sample 115 (offset 8,387,958), i.e. it is deliberately corrupted. 0 packets with unexplained slack; all FSE streams leave exactly 32 bits |

The 20 real files (under ~/Movies): Testfiles/Metric_01.mov (legacy) · KLV Set/15_TURNING_DAISIES_4x_RESTITCH
(DXT1 1920x818, 17,718 fr) · KLV Set/06_THE GREAT SURRENDER (DXT1) · Mac Studio Builds/A 00 Baroque Floral
Vanitas (DXT1 908x2276) · Testfiles/Old Testfiles/dxv3-dxt1 (FFmpeg DXT1) · GlEnc-scratch/ff2997 (DXT1
128x128) · black_snowflake_DXV Normal Quality With Alpha (DXT5 4K) · 1. Indigofera_Full_Film… (DXT5) ·
Local/Cloud Hair Coral Wings… (DXT5 912x2276) · Old Testfiles/DXT5-DXV3_…_6 (DXT5 2048²) ·
GlEnc-scratch/shroomies-clusterfit (DXT5 4K GlEnc) · KLV Set/01_AWAKE & DISSOLVE (YCG6) · KLV Set/16_MIXED
BLOOD (YCG6 480x360) · videos_renamed/A 03 Silver Vortex (YCG6 512x1280) · videos_renamed/00 Golden Cage
(YCG6 908x2276) · hq-16px-test/ycg6-bothN16-1284x724 · Testfiles/YG10-DXV3 · hap-testclips/std_indigofera_h264_audio
(YG10) · hq-16px-test/yg10-bothN16-1284x724 · bad-audio-fixtures/good-audio (YG10). None copied into the repo.

## Speed (MacBook Pro, Apple M3 Max 16-core; Release; 60 frames looped 3 s; ms per frame)
| File | Variant | GPU path, 1 thread | GPU path + jobs | + RGBA, 1 thread | + RGBA + jobs (16) |
|---|---|---|---|---|---|
| KLV 06 (Resolume) | DXT1 1080p | 0.25 | 0.25 | 1.91 | 0.48 |
| Indigofera (Resolume) | DXT5 1080p | 0.49 | 0.49 | 2.59 | 0.80 |
| KLV 01 (Resolume) | YCG6 1080p | 0.96 | 0.75 | 4.70 | 1.21 |
| YG10-DXV3 (Resolume) | YG10 1080p | 1.77 | 1.52 | 6.03 | 2.03 |
| Metric_01 (Adobe) | legacy DXT1 1080p | 1.53 | 1.55 | 3.15 | 1.80 |
| black_snowflake (Resolume) | DXT5 4K | 1.18 | 1.21 | 9.56 | 2.13 |

No 4K DXT1 / YCG6 / YG10 file exists on this Mac, so 4K for those is **estimated as 4x the 1080p figure**
(the work is linear in blocks): GPU path YCG6 ≈ 3.8 ms (≈ 3.0 with jobs), YG10 ≈ 7.1 ms (≈ 6.1 with jobs).
The 4K60 budget is 16.7 ms, so even the worst variant (YG10) uses under half of it on one core. The HQ jobs help only ~1.2x because
section 1 (Y / YA) is ~80 % of the work and its two channels share one literal cursor. Not measured on an
M5 (not available in this session).

## Deviations
- Added `dxvtool check` and a consumption report to the API (needed for the brief's "consumes its bytes exactly").
- The ASan leak checker is unavailable on macOS arm64; leaks were not checked by tool. Every allocation is at
  open / close in mov.c, nothing in the decode path.
- No public FSE / QuickTime / BCn page was fetched; written from general knowledge plus the spec (attested).
- 4K for DXT1 / HQ is extrapolated, not measured (see above).

## QUESTIONS.md (9 items; all settled for the decoder)
1. **Dangling HQ opcode.** This contradicts W1 finding 2: every Resolume YCG6 vector frame leaves 1 opcode (opcode 3, 8 literal bytes)
   in the Y channel. On real Resolume YCG6 files 156,100 of 161,737 packets have such a tail, and their literal bytes always
   equal the unread literal area. Decoder ignores it (spec 7.5). **For Phase A:** fix the W1 wording / sharpen 7.5.
2. FSE spread step **641** confirmed by experiment (643 breaks all 8 Resolume HQ rows).
3. Selector 3 never occurs in any vector (only 2 / 0 / 1); treated like 2. **For Phase A:** confirm.
4. Literals past the opcode-area offset are rejected (strict SHOULD); no vector or real file trips it.
5-9. HQ + stored flag rejected; only DXD3 / DXDI accepted; premultiply = (c·a+127)/255; max 16384 per axis;
   count extensions bounded by the payload.

## Attestation (full text in PROVENANCE.md)
Fresh session (a1c3b38d-05d5-4958-b2d4-ab4bb740f08e). It read only this repo's CLAUDE.md, spec, handoffs, cleanroom docs,
vectors and README, plus real DXV files as data. It **did not access** FFmpeg, Glance / GlanceCore / GlancePlayback, glenc-public,
GlEnc, CueGlance, MaCrate, Alley internals or any DXV implementation; it made no web searches and had no accidental exposure. No
third-party code was vendored.

## Needs Amrit
- Nothing blocks the decoder. Still open from W1: straight vs premultiplied alpha in Arena (WHAT: compare an Alley
  and an AME DXT5 alpha clip of the same source over a background; WHERE: Arena). DXVKit defaults to untouched.
- Optional: a 4K YCG6 / YG10 Alley export (any content) in `~/Movies/Testfiles/` would replace the 4K estimate
  with a measurement.
- Amrit's answers from this session: none. No questions were asked.

## RECEIPTS
```
$ git log -1 --format="%H %an %ad%n%s"; git status -sb | head -1; git ls-remote origin main
cdbb4bc0f2407d7c3c88d4b050592e0e1fec2da7 Thu Oct 8 13:02:32 2026 +0200
DXVKit W2 (Phase B, clean room): C99 DXV decoder, C++ wrapper, SwiftPM package, dxvtool
## main...origin/main
cdbb4bc0f2407d7c3c88d4b050592e0e1fec2da7	refs/heads/main
$ build/dxvtool verify test_vectors/vectors.tsv   (per group; full table in the tool output)
  56 glenc decode ok
  11 invalid reject ok
  16 resolume decode ok
  19 synthetic decode ok
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
$ (cd build && ctest)
1/3 Test #1: vectors ..........................   Passed    0.54 sec
2/3 Test #2: vectors_threaded .................   Passed    0.51 sec
3/3 Test #3: cpp_wrapper ......................   Passed    0.07 sec
100% tests passed, 0 tests failed out of 3
$ swift test
	 Executed 2 tests, with 0 failures (0 unexpected) in 0.651 (0.653) seconds
$ ASan+UBSan build (build-asan, Debug, -fno-sanitize-recover=all)
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
rows 102 | block streams 127/127 identical | rgba 91/91 identical | rejected 11/11 | failures 0
# libFuzzer (start / end UTC), per-worker final lines
2026-10-08T10:28:54Z
2026-10-08T10:59:00Z
packet_run: crash/leak/timeout/oom artifacts: 0  sanitizer ERROR lines: 0
#1320300	DONE   cov: 522 ft: 2063 corp: 727/53Mb lim: 305232 exec/s: 733 rss: 43
#1264545	DONE   cov: 522 ft: 2063 corp: 721/54Mb lim: 305232 exec/s: 702 rss: 44
#1329282	DONE   cov: 522 ft: 2063 corp: 732/53Mb lim: 305232 exec/s: 738 rss: 43
#1342552	DONE   cov: 522 ft: 2063 corp: 732/54Mb lim: 305232 exec/s: 745 rss: 44
#1298512	DONE   cov: 522 ft: 2063 corp: 721/53Mb lim: 305232 exec/s: 720 rss: 43
#1338813	DONE   cov: 522 ft: 2063 corp: 720/52Mb lim: 305232 exec/s: 743 rss: 43
mov_run: crash/leak/timeout/oom artifacts: 0  sanitizer ERROR lines: 0
#41100408	DONE   cov: 570 ft: 2235 corp: 1143/42Mb lim: 595223 exec/s: 22820 rss
#40258482	DONE   cov: 570 ft: 2235 corp: 1141/43Mb lim: 595223 exec/s: 22353 rss
#41819408	DONE   cov: 570 ft: 2235 corp: 1152/41Mb lim: 595223 exec/s: 23220 rss
#40692946	DONE   cov: 570 ft: 2235 corp: 1146/43Mb lim: 595223 exec/s: 22594 rss
#40683094	DONE   cov: 570 ft: 2235 corp: 1135/42Mb lim: 595223 exec/s: 22589 rss
#39840260	DONE   cov: 570 ft: 2235 corp: 1146/44Mb lim: 595223 exec/s: 22121 rss
$ dxvtool check <20 real files> (last line)
files 20 (bad 0) | packets 45262 | errors 0 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 13408
$ dxvtool check <all 426 DXV files in ~/Movies> (last line)
files 426 (bad 1) | packets 320888 | errors 1 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 156100
$ lipo -info; ls -la
Architectures in the fat file: build/dxvtool are: x86_64 arm64 
Architectures in the fat file: build/libdxvkit.a are: x86_64 arm64 
Architectures in the fat file: .build/out/Products/Release/DXVKit.o are: x86_64 arm64 
-rwxr-xr-x 135768 build/dxvtool
-rw-r--r-- 74560 build/libdxvkit.a
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256 | grep -vc ": OK$")
0
Wire / Arena not touched.
```
END OF HANDOFF w2-cleanroom-results sha256:012965b9 11518
