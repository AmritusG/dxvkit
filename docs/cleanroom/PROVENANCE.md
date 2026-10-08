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
