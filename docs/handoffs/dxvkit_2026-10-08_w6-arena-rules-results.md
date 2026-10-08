project:    dxvkit
pass:       w6-arena-rules-results
STATUS:     COMPLETE
parent:     681f760e0b75164a2ce89b28f0d60575dbfaa51c
result:     2f5256ab2c2b10bdfebe68275129dbf51e48fd10 (tag v1.0.0)
session:    5724d148-6d5f-4953-9120-562ba246ddf6
written_at: 2026-10-08T19:05:09Z

# DXVKit W6 results: Arena 7.28 rules (F1-F7), tag v1.0.0 (clean room)

Brief: `docs/handoffs/dxvkit_2026-10-08_w6-arena-rules-brief.md`. All seven findings were done as
decided. Tag **v1.0.0** = commit 2f5256a, pushed. This is the tag the apps pin.

## What changed, per finding
- **F1** Legacy type byte: 0x40 means DXT5, anything else DXT1, and no type byte is rejected. The old
  reject vector `bad_dxdi-no-texture-flag` is now the decode vector
  `synthetic-dxdi_type03-implied-dxt1_pat64`. Its blocks equal the 0x23 control's.
- **F2 / F3** A DXV3 version byte below 4, or a stored flag above 1, is rejected with a new status,
  `DXVK_ERR_BAD_HEADER`. It is appended to the enum, so existing status values keep their numbers.
- **F4** Stored YCG6 / YG10 packets decode: plane 0, then plane 1.
- **F5** `DXD4` is accepted. The packet decides legacy vs DXV3 (a DXV3 tag in its first 4 bytes means
  DXV3). The `generation` argument is now only a hint, and `DXVK_GEN_AUTO` was added.
- **F6** HQ RGBA path (C core, so C++ and Swift too) uses Arena's convention:
  - exact BC4 palettes, done in integer fixed point;
  - nearest chroma, midpoint 127.5;
  - R / B overflow folding, rounded at the end.

  Before coding I checked which palette fits Arena's capture: the exact one (97.21 % within 1) beats
  the integer one (94.24 %). The 39 HQ RGBA references were regenerated. Block references did not change.
- **F7** Encoder (both efforts): exact, unrounded Y, Co and Cg, chroma midpoint 127.5, 2 x 2 box
  average. This uses a new BC4 encoder for fractional targets, measured against the exact palette.
  DXT1 / DXT5 output is byte-identical to before on 8 test encodes, and the alpha plane and the
  blocks-in path are untouched.
- Version 1.0.0 in the C header, C++, Swift (`DXVKitVersion`) and CMake. Added `CHANGELOG.md` and
  three `dxvtool` commands: `arena`, `dump-packet`, `version`.
- Vectors: the 33 W5 forms (3.2 MB, `cp -p`) are in `test_vectors/streams/arena/`, and
  `arena_forms.tsv` holds Arena's verdict for each. Totals: 334 rows, 80 files, 750 manifest entries.

## Validation
- **Arena agreement:** 33 / 33 files get the same verdict from DXVKit and Arena (play / black / not a
  clip), 0 disagreements. Before W6 it was 24 / 33 (forms 10, 14, 17, 19, 21, 23, 27, 28, 31
  disagreed). Every form Arena plays decodes to exactly its control's block streams.
- **Old vectors:** 0 of 105 old rows changed a block-stream hash; only the 39 HQ `rgba=` hashes changed.
- **Colour vs Arena** (W5 Syphon capture, chroma-edge file, all 6,220,800 colour bytes):

  | | exact | within 1 | within 2 | max |
  |---|---|---|---|---|
  | DXVKit 1.0 | 79.62 % | 97.21 % | 98.38 % | 11 |
  | old rule | 32.26 % | 85.35 % | — | 54 |

  YCG6 and YG10 give identical output. The W5 model reported 97.24 % within 1 on its own lattice.
- **Grey 128:** the 1.0 encoder writes chroma endpoints 128 / 127 with entry 127.571, and the 8.4 model
  shows 128 / 128 / 128. The W3 file under the same model shows 128 / 129 / 127, which is exactly what
  W5 captured from Arena for it. Both files are loaded in Arena (layer 8, columns 37 and 38), not
  triggered. No capture was taken (see below).
- **Encoder round trip:** 112 / 112 cases identical (ctest, also under ASan).
- **Fair Alley comparison** (W3 source, 3 frames, mean packet bytes / PSNR RGB):
  - DXT1 / DXT5: identical to W3.
  - YCG6 / YG10 fast: 132,336 B, 37.90 dB under Arena's rule. W3 encoder: 123,399 B / 37.55 dB.
    Alley: 132,032 B / 37.81 dB.
  - Best effort: 132,064 B / 38.06 dB.

  So quality as Arena shows it is up 0.35 / 0.40 dB, and size is up 7.2 %, which is level with Alley.
- **ASan + UBSan:** ctest 4 / 4; verify (4 threads) 0 failures; arena 33 / 33.
- **libFuzzer, 30 min each, 0 crashes:** packet decode 1,613,732 runs, encode 236,035 runs, MOV
  (extra) 12,211,891 runs.
- **Real files:** `dxvtool check` over all 434 DXV files in ~/Movies (127,920 packets) found 33
  errors, all in known fixtures:
  - 32 packets: the 4 Arena-"black" forms in `Testfiles/dxvkit-w5-forms` (14, 17, 29, 31), as intended;
  - 1 packet: `Testfiles/bad-audio-fixtures/bad-audio.mov` frame 115, the deliberately corrupted file
    W2 already recorded.

  Every other packet consumed its bytes exactly, apart from the documented dangling-opcode tail.

## Deviations
- YCG6 packets grew 7.2 % against the W3 encoder because Y is now exact as well as chroma (the brief
  says "exact unrounded YCoCg"). Rounding Y to integers would save 4.6 % at -0.08 dB. It is QUESTIONS
  W6-2 and needs your choice.
- I did not edit the spec. Three sentences that describe DXVKit's old state are listed for a Phase A
  session (QUESTIONS W6-1).
- The W5 Syphon captures were not in the repo or listed by path. I read them as data from W5's scratch
  folder (recorded in PROVENANCE).
- Swift test counts were stale (91 / 127 / 11 from W2). They are now 287 / 366 / 47.

## ADOPTION NOTE for Glance, GlEnc, CueGlance, MaCrate, LoadEhr
Pin tag `v1.0.0` (commit 2f5256a). Changes vs cdbb4bc (the W2 decoder) for a caller:
1. **Acceptance.** These now decode: every legacy type byte, stored HQ packets, `DXD4`, and
   FourCC / packet mismatches. These are now rejected: DXV3 version byte below 4, stored flag above 1
   (`DXVK_ERR_BAD_HEADER`, a new enum value). `DXVK_ERR_UNSUPPORTED` / `UNKNOWN_VARIANT` are no
   longer returned by the decoder. Passing `DXVK_GEN_AUTO` is fine; the packet decides.
2. **HQ RGBA midpoint.** The CPU RGBA output for YCG6 / YG10 changed: chroma midpoint 127.5, exact
   palettes, folding. DXT1 / DXT5 RGBA did not change.
3. **Encoder midpoint.** HQ files are now written with chroma midpoint 127.5, like Alley.
4. **GPU-path callers** (shaders): to match Arena, the chroma midpoint must be 127.5 / 255 = 0.5, not
   128 / 255, and chroma must be sampled nearest (point). Folding R / B overflow into the other channel
   matters only for saturated colours.

## Attestation
Clean room. I read only this repo, Arena's MCP (read-only, plus 2 untriggered clip loads), the ffmpeg
binary and data files. No FFmpeg / Glance / GlEnc / CueGlance / MaCrate / LoadEhr source was read.
Details are in PROVENANCE.md (W6). Licences: nothing new except our own files.

## Amrit's answers / needs your eyes
- No questions were asked this session.
- **Optional:** to confirm F7 with a capture, fire Arena layer 8 column 37 (and 38 for comparison)
  with Syphon on, and a new session can read the pixels. I don't need it for the release.
- **Choice (W6-2):** exact Y (shipped, Alley-like) or integer Y (4.6 % smaller, -0.08 dB)?
- **Arena:** layer 8 now also holds columns 37 and 38 (grey files from `~/Movies/Testfiles/dxvkit-w6-grey/`).
  The composition was not saved.

## RECEIPTS
```
$ git log -1 --format='%H %s'
2f5256ab2c2b10bdfebe68275129dbf51e48fd10 DXVKit 1.0.0 (W6, clean room): Resolume Arena 7.28 rules (F1-F7)
$ git ls-remote origin main refs/tags/v1.0.0 'refs/tags/v1.0.0^{}'
2f5256ab2c2b10bdfebe68275129dbf51e48fd10	refs/heads/main
2aa313aff45d66e334097202913531938e48e9c5	refs/tags/v1.0.0
2f5256ab2c2b10bdfebe68275129dbf51e48fd10	refs/tags/v1.0.0^{}
$ build/dxvtool verify test_vectors/vectors.tsv | tail -1
rows 334 | block streams 366/366 identical | rgba 287/287 identical | rejected 47/47 | failures 0
$ build/dxvtool arena test_vectors/arena_forms.tsv | tail -1      (before W6, old build: 24/33)
files 33 | agree 33 | disagree 0
$ (cd build && ctest) ; (cd build-asan && ctest) ; swift test
100% tests passed, 0 tests failed out of 5   (build)
100% tests passed, 0 tests failed out of 4   (build-asan, DXVK_SANITIZE=ON)
	 Executed 4 tests, with 0 failures (0 unexpected) in 1.143 (1.144) seconds
$ build/dxvkit_encoder_test | tail -1
round trip: 112 cases, 0 failures
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256) | grep -c ': OK$'
750
$ fuzz (libFuzzer, Homebrew LLVM, ASan+UBSan, started 2026-10-08T18:31:01Z, -max_total_time=1800)
packet: Done 1613732 runs in 1801 second(s) | exit 0
encode: Done 236035 runs in 1801 second(s) | exit 0
mov:    Done 12211891 runs in 1801 second(s) | exit 0
crash artifacts: 0
$ dxvtool check (434 DXV files under ~/Movies, --threads 8)
files 434 (bad 5) | packets 127920 | errors 33 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 43652
$ colour vs Arena Syphon capture (1920x1080 chroma-edge YCG6, 6,220,800 colour bytes)
DXVKit 1.0 YCG6: colour bytes 6220800 exact 79.62% <=1 97.21% <=2 98.38% max 11 mean 0.267 
W2 rule YCG6: colour bytes 6220800 exact 32.26% <=1 85.35% <=2 95.29% max 54 mean 1.128
$ fair Alley comparison (variant | encoder | mean packet B | PSNR W2 rule | PSNR 1.0 Arena rule)
ycg6 | Alley | 132032 | 37.714 | 37.812
ycg6 | W3 fast | 123399 | 37.951 | 37.551
ycg6 | 1.0 fast | 132336 | 37.640 | 37.903
ycg6 | W3 best | 123218 | 38.026 | 37.661
ycg6 | 1.0 best | 132064 | 37.710 | 38.058
$ ls -la test_vectors/streams/arena | wc -l ; du -sh test_vectors test_vectors/streams/arena
33 files ;  18M test_vectors ; 3.2M arena
$ ls -la ~/Movies/Testfiles/dxvkit-w6-grey
-rw-r--r--   1 amritrosell  staff   701 Oct  8 20:35 dxvkit-w6_grey128_ycg6-v1.0_1920x1080.mov
-rw-r--r--   1 amritrosell  staff   701 Oct  8 20:35 dxvkit-w6_grey128_ycg6-w3_1920x1080.mov
Arena: comp "LoadEhr" (not saved), layer 8 C37/C38 loaded, nothing triggered; Wire not touched.
```
END OF HANDOFF w6-arena-rules-results sha256:920537a6 9332
