# Clean-room provenance log

DXVKit is written under a two-room protocol (see `CLAUDE.md`, HARD RULES). This log records what each
room read. Phase B appends its own attestation below; nobody edits an earlier entry.

## Phase A: the dirty room (spec writer)

- Session: Claude Code (Claude Opus 5.5), session `session_01LxGX4yxKucdPVDMKd9cfmG`, on Amrit's MacBook Pro
  (M3 Max), working directory `~/Documents/claude/DXVKit`.
- Date: 2026-10-08, roughly 08:45-10:30 UTC.
- Brief: `docs/handoffs/dxvkit_2026-10-08_w1-spec-brief.md` (by Cowork).

### Read

| What | Version / location | Why |
|---|---|---|
| FFmpeg `libavcodec/dxv.c`, `dxv.h`, `dxvenc.c`, `texturedsp.c`, `texturedsp.h`, `texturedsp_template.c`, `texturedspenc.c`, `lzf.c` | current master, fetched 2026-10-08 08:52 UTC from raw.githubusercontent.com | the decoder / encoder behaviour |
| FFmpeg `dxv.c`, older copy | `~/Downloads/dxv.c` (2026-05-05). Only difference from master: coded width aligned to 4 instead of 16 | "the version Glance tracks" |
| glenc-public (GlEnc v1.3.0 public mirror, commit 8e73f3a) | `~/Documents/claude/native-builds/glenc-public`: `Sources/GlanceCore/` (DXVDetector, DXV1PacketDecoder, DXVPacketDecoder, DXVHQDecoder, DXVHQOpcodeDecoder, DXVHQCgoDecoder, BC4BC5Unpack, CPURender, DXVDemuxer, DXVThumbnail), `Sources/GlEncCore/` headers and comments (DXVFormat, YCoCgTransform, AlphaNormalization, DXVHQOpcodeWriter, DXVHQCgoWriter), `THIRD-PARTY-NOTICES.md`, `Package.swift`; `GlancePlayback/DXVRenderer.swift` (the YCoCg shader comment) | Glance / GlEnc behaviour, known quirks |
| GlEnc reference corpus notes | `~/Documents/claude/native-builds/GlEnc/reference/README.md`, `dxt5/FINDINGS.md` (alpha section, grep only), `ycg6/FINDINGS.md` (grep only) | where the Resolume Alley / AME exports came from |
| Resolume Alley 7.28 bundle | `/Applications/Resolume Alley/Licenses/ycocg.md` (the FSE and YCoCg licence notices) | which entropy coder family DXV HQ uses |
| Real DXV files | 695 files on this Mac (see `INVENTORY.md`): container fields, every packet header, padding / consumption measurements, opcode statistics, alpha statistics | ground truth |

### Wrote

- `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md`: prose and tables only.
- `test_vectors/`: vector files and reference outputs (data). The reference outputs come from Glance's
  decoder, run through a dump tool that lived only in the Phase A scratch folder
  (`/private/tmp/claude-501/…/scratchpad/dxvdump`, never committed). Synthetic and invalid vectors were
  built with scratch-only scripts (a minimal QuickTime writer, an LZF compressor); none of that code is in
  the repository.
- A scratch-only checker written from the Phase A notes (not from upstream code) reproduced all 127
  reference block streams of the valid vectors byte for byte and rejected all 11 invalid vectors: evidence
  that the prose in DXV_FORMAT.md is complete. The checker is not committed either.
- `CLAUDE.md`, `README.md`, `LICENSE`, this file, `INVENTORY.md`, the W1 handoffs.

### Not written

No decoder or encoder source code, no pseudocode, no upstream identifiers, comments or line references in
the spec (self-check receipts in the W1 results handoff).

### Generation of our own source pattern (for the GlEnc vectors)

FFmpeg 8 (Homebrew), `lavfi color` source, then the expression below (written for this project) per frame
N, saved as PNG, then ProRes 4444 for GlEnc's input:

- R = (9 X + 40 N) mod 256
- G = (13 Y + 20 N) mod 256
- B = 230 on the 4 x 4 checker cells where floor(X/4) + floor(Y/4) is even, else (5 (X + Y)) mod 256
- A = 255 for X < W/4; 0 for X < W/2; 128 for Y < H/2; else (11 X + 3 Y) mod 256

## Phase B: the clean room (implementer)

(Phase B appends here: session, date, what it read, an attestation that it did not open FFmpeg, Glance,
GlEnc, CueGlance or MaCrate source.)

### Phase B attestation (W2, the clean room)

- Session: Claude Code (Claude Opus 5.5), session `a1c3b38d-05d5-4958-b2d4-ab4bb740f08e`, on Amrit's
  MacBook Pro (Apple M3 Max, 16 cores, 48 GB), working directory `~/Documents/claude/DXVKit`. Fresh
  session: its context held nothing from Phase A when it started.
- Date: 2026-10-08, 10:14-11:02 UTC.
- Brief: `docs/handoffs/dxvkit_2026-10-08_w2-cleanroom-brief.md` (by Cowork).

#### Read

| What | Why |
|---|---|
| `CLAUDE.md` (this repo) and Amrit's global `~/.claude/CLAUDE.md` (handoff rules only) | rules |
| `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md` (in full) | the specification |
| `docs/handoffs/dxvkit_2026-10-08_w1-spec-results.md`, `dxvkit_2026-10-08_w2-cleanroom-brief.md` | findings, brief |
| `docs/cleanroom/PROVENANCE.md`, `docs/cleanroom/INVENTORY.md`, `README.md`, `test_vectors/README.md`, `test_vectors/vectors.tsv` | provenance, vector layout |
| `test_vectors/streams/*`, reference hashes | test data (decoded, compared by SHA-256) |
| Real DXV files under `~/Movies` (426 files, read as data by our own decoder; none copied) | real-file test, benchmarks |
| Public knowledge of the QuickTime file format (atoms, `stsd`, `stts`, `stsz`, `stsc`, `stco` / `co64`), S3TC / RGTC (BC1 / BC3 / BC4 / BC5), tANS / FSE decoding as in RFC 8878 section 4.1 and Yann Collet's FSE write-ups, the liblzf format, FIPS 180-4 (SHA-256, for the tool) | written from general knowledge; no web page was fetched and no web search was made in this session |

#### Did not access

FFmpeg (no libavcodec file, mirror or documentation page), Glance / GlanceCore / GlancePlayback,
glenc-public, GlEnc, CueGlance, MaCrate, Alley internals, or any other DXV implementation, source or
summary. No web search of any kind. No other session was asked about upstream code. The Phase A
scratch tools (dump tool, checker, QuickTime writer, LZF compressor) were not opened.

#### Accidental exposure

None. (Some DXV files on disk were written by GlEnc or FFmpeg; they were decoded as data only.)

#### Wrote

All code in `src/`, `include/`, `swift/`, `tests/`, `tools/`, `CMakeLists.txt`, `Package.swift`, plus
`docs/design.md`, `docs/cleanroom/QUESTIONS.md` and this entry, from the documents above. No
third-party code is vendored: the FSE decoder, LZF decoder, QuickTime demuxer and SHA-256 are our own.
Questions where the spec was silent or ambiguous were settled from the test vectors only
(`docs/cleanroom/QUESTIONS.md`).

### Phase B attestation (W3, the clean-room encoder)

- Session: Claude Code (Claude Opus 5.5), session `a3fd4145-83a7-4e1e-931f-72534cdaed15`, on Amrit's
  MacBook Pro (Apple M3 Max, 16 cores), working directory `~/Documents/claude/DXVKit`. Fresh session
  after /clear: its context held nothing from FFmpeg, Glance or GlEnc source when it started.
- Date: 2026-10-08, from about 13:05 UTC (see the W3 results handoff for the end time).
- Brief: `docs/handoffs/dxvkit_2026-10-08_w3-encoder-brief.md` (by Cowork).

#### Read

| What | Why |
|---|---|
| `CLAUDE.md`, `README.md`, Amrit's global `~/.claude/CLAUDE.md` (handoff rules) | rules |
| `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md` (in full), `docs/design.md`, `docs/cleanroom/QUESTIONS.md`, `PROVENANCE.md`, `INVENTORY.md` | the specification and the W2 decisions |
| `docs/handoffs/` W1 and W2 results, W3 brief | findings, brief |
| DXVKit's own decoder: `src/*.c`, `src/dxvk_priv.h`, `include/`, `tools/`, `tests/`, `swift/` (clean-room MIT code from W2) | to mirror the decoder exactly and keep the style |
| `test_vectors/` (streams, reference hashes, README) | round-trip, repack and Alley comparisons (data) |
| One real Resolume file as data: `~/Movies/Mac Studio Builds/A 00 Baroque Floral Vanitas 10s Loop.mov` (908 x 2276 DXT1), decoded by our decoder and by the ffmpeg binary | the ffmpeg geometry check (QUESTIONS 12); not copied |
| Public knowledge of S3TC / RGTC (BC1 / BC3 / BC4 / BC5), tANS / FSE encoding (Yann Collet's FSE write-ups, RFC 8878 section 4.1), the QuickTime file format, the PNG specification, the PAM / PPM formats | written from general knowledge; no web page was fetched and no web search was made |

#### Black-box use (outputs only, never source)

| Program | Use |
|---|---|
| `ffmpeg` / `ffprobe` 8.1.1 (Homebrew binaries, not linked, not committed) | generated synthetic sources (`testsrc2`, `geq`); decoded our encoder's files to RGBA / yuv(a)420p and compared them with our decoder (`tools/oracle.sh`); decoded the Resolume vectors and the 908 x 2276 Resolume file for comparison; probed stream info. Findings: QUESTIONS 10-13 |
| Resolume Arena 7.28 (via the Arena MCP server) | added layer 6 "DXVKit Encoder" to the open composition and loaded 10 encoder outputs into columns 1-10 (not triggered, nothing else touched, composition not saved); read clip info, thumbnails and diagnostics |
| Resolume Alley | not used: it has no command line and could not be driven without its GUI |

#### Did not access

FFmpeg source (no `dxv.c`, `dxvenc.c`, `texturedsp*`, mirror, doxygen page or documentation of their
internals), Glance / GlanceCore / GlancePlayback, glenc-public, GlEnc (including its reference corpus
folder and its README / FINDINGS files), CueGlance, MaCrate, Alley internals, or any other DXV
implementation, source or summary. No web search of any kind. No other session was asked about
upstream code. The Phase A scratch tools were not opened.

#### Accidental exposure

None.

#### Wrote

Encoder code: `src/enc.c`, `src/enc_bc.c`, `src/enc_pack.c`, `src/enc_hq.c`, `src/enc_priv.h`,
`src/movwrite.c`, `src/enc_tables.h` (generated by our `tools/gen_bc_tables.c`); additions to
`include/dxvkit.h`, `include/dxvkit.hpp`, `swift/Sources/DXVKit/DXVEncoder.swift`, Swift and C++
tests, `tests/encoder_test.c`, `tools/dxvtool.c` (encode, bench-encode, repack, psnr, cmp-rgba,
dump-samples, check --histogram), `tools/imageio.c` (PNG / PAM / PPM input via the system zlib, zlib
licence, used by dxvtool only), `tools/fuzz_encode.c`, `tools/oracle.sh`, `docs/design-encoder.md`,
QUESTIONS 10-20 and this entry. No third-party code is vendored: the BCn encoders, the FSE encoder and
the QuickTime writer are our own.
