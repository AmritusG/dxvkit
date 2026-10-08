# DXVKit design (Phase B, W2)

Written by the clean-room session from `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md`,
`test_vectors/` and public container / BCn / FSE documentation only. Spec section numbers are given as
"spec 7.3".

## What the spec asks of a decoder (extract)

| Area | Requirement (spec section) |
|---|---|
| Container | QuickTime; one DXV video track, sample-entry FourCC `DXD3`, `DXDI` or `DXD4` (W6; any other FourCC is not DXV); entry width / height = display size (2.2). Variant and generation are in every packet, not in the container. |
| Coded size | CW = ceil16(W), CH = ceil16(H), every variant (2.3). Chroma planes CW/2 x CH/2. |
| DXV3 header | 12 bytes: tag (`DXT1`/`DXT5`/`YCG6`/`YG10` as LE u32 = BE FourCC value), version, minor, stored flag, reserved, u32 payload length (3.1). Reject: < 12 bytes, unknown tag, length + 12 > packet, version byte < 4, stored flag not 0 / 1 (3.2, W6). Accept trailing bytes. |
| Stored flag | Payload is the block stream(s), at least as long as needed (3.3). HQ (W6): plane 0 then plane 1, back to back. |
| Legacy | u32 type word: bits 0..23 length, 24..27 version nibble, 0x40 top-byte DXT5, else DXT1 (W6: no type byte is rejected); 0x80 = stored, else LZF; LZF output length must equal the texture exactly (4). |
| DXT1 | block-copy layer, 2-bit flags packed 16 per u32, distances 1 / u8+2 / u16+258 in blocks; halves of 4 bytes (5). |
| DXT5 | same flags; alpha commands (block run, alpha run, u16+2 back, literal); colour half in quarters; count extension (6). |
| HQ | two plane sections, each: op-area offset, opcode count(s), literal area, opcode stream(s) stored / fill / FSE (table log 10, spread step 641, bitstream read downwards from bit L-1); 18 opcodes over BC4 blocks with two 256-slot hashed dictionaries per channel (7). |
| Pixels | BC1 bit-replicated 565, truncating /3 and /2 palettes, DXT1 opaque (alpha 255, index 3 black); BC3 alpha / BC4 palettes; BC3 colour always four-colour; YCoCg plain inverse with nearest-neighbour chroma (8); W6: chroma midpoint 127.5, exact BC4 palettes, R / B overflow folding, rounding at the end (8.4). Alpha values untouched (8.5). |
| Errors | the checklist of spec 9; every read bounds-checked; no state between packets. |

## Architecture

```
include/dxvkit.h      C99 API (the only public C header)
include/dxvkit.hpp    header-only C++17 RAII wrapper
include/module.modulemap   Clang module "CDXVKit" (SwiftPM), exposes dxvkit.h only
src/                  C99 core, libc only
  dxvk_priv.h         internal helpers (little-endian loads, internal prototypes)
  packet.c            header parsing, variant / plane geometry, dispatch, stored payloads, status strings
  legacy.c            DXDI type word + LZF
  blockcopy.c         DXT1 and DXT5 block-copy layers
  hq.c                YCG6 / YG10 plane sections, opcode streams (stored / fill / FSE), block opcodes
  pixels.c            BC1 / BC3 / BC4 / YCoCg -> RGBA8, optional premultiply
  mov.c               QuickTime demuxer
swift/                SwiftPM sources (Package.swift at the repo root): module DXVKit
tests/                C++ wrapper test (ctest)
tools/dxvtool.c       CLI: info / dump-blocks / dump-rgba / verify / check / bench (+ its own SHA-256)
tools/fuzz_*.c        libFuzzer harnesses (packet decode, MOV demux) and the seed-corpus writer
```

### Principles

- **No global state, no allocation in the decode path.** Packet decode and RGBA conversion work only on
  caller buffers plus stack memory (the largest stack user is one HQ section: four FSE tables of 1024
  entries x 4 bytes plus the dictionaries, under 20 KB). The demuxer allocates its sample table once at
  open (`malloc`), never afterwards.
- **Streaming opcodes.** HQ opcode streams are decoded on demand while blocks are reconstructed (an FSE
  state per channel), so no opcode buffer is needed. When a channel's blocks are complete, the opcodes
  it did not use are still decoded (and discarded), so a malformed bitstream tail is detected exactly as
  if everything had been decoded up front.
- **Sections are located before they are decoded.** Every opcode stream's length is known from its
  first bytes (stored: 1 + count, fill: 2, FSE: table length + area length), so section 2's start is
  found without reconstructing section 1. The two sections are therefore independent jobs.
- **Parallelism by callback.** `dxvk_jobs` is a caller-supplied "run N jobs, return when all done"
  function. The core uses it for the two HQ sections and for RGBA conversion in row bands. With no
  callback everything runs on the calling thread. The core never creates threads.
- **Strict bounds.** Every length is checked before use, in size_t arithmetic with explicit overflow
  guards; texture copies are block-index based, so a distance is checked once against the block index.
- **Consumption report.** Each decode can fill an optional `dxvk_consumption` (bytes consumed versus
  payload, unused literals / opcodes, FSE bits left) so tools can prove "every packet consumes its bytes
  exactly" on real files.

### Output layout (output A, the GPU path)

| Variant | plane 0 | plane 1 |
|---|---|---|
| DXT1, legacy DXT1 | BC1, (CW/4)x(CH/4) blocks, 8 B | none |
| DXT5, legacy DXT5 | BC3, 16 B blocks | none |
| YCG6 | Y as BC4, 8 B blocks, (CW/4)x(CH/4) | Co+Cg as BC5-layout 16 B blocks, (CW/8)x(CH/8) |
| YG10 | Y+A as BC5-layout 16 B blocks (Y bytes 0..7, A 8..15), (CW/4)x(CH/4) | Co+Cg as plane 1 of YCG6 |

Blocks row by row, top row first; the whole coded (padded) texture. Identical to the reference files.

### Output B (CPU)

RGBA8, display size W x H, caller stride, cropped from the coded texture, the conventions of spec 8.
Option `DXVK_OPT_PREMULTIPLY`: c' = (c x a + 127) / 255 per colour channel (round to nearest), applied
only to variants with alpha. Default: values untouched.

### Limits (our choices, not from the spec)

- Display width and height 1..16384 (the spec has nothing above 16384; GPU texture limits are the same).
- MOV: at most 2^24 samples; `moov` at most 64 MB (read into memory once at open); every sample must
  lie inside the file (checked at open); the first DXV track is used.
- Accepted sample-entry FourCCs: `DXD3`, `DXDI`, `DXD4` (W6, what Arena 7.28 loads; spec 2.2). Others are
  not DXV.

## W6: Arena 7.28 rules (findings F1-F7, clean room)

Written by the W6 clean-room session from `docs/spec/DXV_FORMAT.md` (2.2, 3.1-3.3, 4.1, 8.1, 8.2, 8.4,
9, 10), `docs/spec/ENCODING_NOTES.md` 5, `docs/cleanroom/QUESTIONS.md` F1-F7 and the W5 results handoff.
Amrit's decisions are in the W6 brief (`docs/handoffs/dxvkit_2026-10-08_w6-arena-rules-brief.md`); the
authority is Resolume Arena 7.28 as a black box.

### What the spec now asks (extract)

| # | Spec | Rule | DXVKit before W6 |
|---|---|---|---|
| F1 | 4.1 | Legacy texture: type-byte bit 0x40 -> DXT5, otherwise DXT1. No type byte is rejected; reject only a packet < 4 bytes, length + 4 > packet, and payload errors (4.2) | rejected 0x03 (no texture bit, nibble 3) |
| F2 | 3.1, 3.2 | DXV3 version byte (byte 4) below 4: MUST reject. 4 and above, any minor (byte 5), any reserved (byte 7): accept | ignored the version byte |
| F3 | 3.1, 3.2 | DXV3 stored flag (byte 6) other than 0 / 1: MUST reject, whatever the payload | treated any non-zero value as stored |
| F4 | 3.3 | Stored `YCG6` / `YG10`: payload = plane 0 (Y BC4 or Y+A BC5) immediately followed by plane 1 (Co+Cg BC5); at least that long, extra ignored | rejected (`DXVK_ERR_UNSUPPORTED`) |
| F5 | 2.2, 9 | Accept the sample-entry FourCCs `DXD3`, `DXDI`, `DXD4`, nothing else. The packet decides the generation: first four bytes = one of the four DXV3 tags -> DXV3, otherwise legacy | `DXD3` / `DXDI` only; the FourCC chose the generation |
| F6 | 8.4 | Y, Co, Cg (and A) from the BC4 blocks with the **exact** palette (no truncation); nearest chroma (x / 2, y / 2); co = Co - 127.5, cg = Cg - 127.5; R = Y - cg + co, G = Y + cg, B = Y - cg - co; fold: R < 0 -> B += R, R = 0; B < 0 -> R += B, B = 0; R > 255 -> B += R - 255, R = 255; B > 255 -> R += B - 255, B = 255 (in that order); then round to nearest and clamp | integer palettes, midpoint 128, independent clamping |
| F7 | ENCODING_NOTES 5 | Forward: Y = R/4 + G/2 + B/4, Co = R/2 - B/2 + 127.5, Cg = -R/4 + G/2 - B/4 + 127.5, exact and unrounded into the BC4 encoder; chroma = 2 x 2 box average | Y rounded to an integer, chroma rounded, midpoint 128 |

Measured in W6 before coding, to settle the one thing 8.4 leaves implicit (exact or integer palette):
against W5's Syphon capture of the chroma-edge `YCG6` file (all 6,220,800 colour bytes of the frame), the
exact palette with 127.5 + folding gives 79.62 % exact / 97.21 % within 1 / max 11; the truncating integer
palette with 127.5 + folding 71.84 % / 94.24 % / max 11; the W2 rule (integer, 128, clamping) 32.26 % /
85.35 % / max 54. The exact palette is used. (On W5's quarter lattice the exact model gives 97.10 % within
1; W5 reported 97.24 % on its own lattice.)

### Design

- **Generation is the packet's.** `dxvk_decode_blocks` and `dxvk_packet_variant` keep their
  `dxvk_generation` argument for source compatibility, but the packet decides (2.2): a packet of at least
  4 bytes whose first four bytes are a DXV3 tag is DXV3, any other packet is legacy. `DXVK_GEN_AUTO` is
  added for callers without a container; all three values behave the same. `dxvk_mov_info.generation`
  stays the container's hint (`DXDI` -> legacy, `DXD3` / `DXD4` -> DXV3). A legacy packet whose
  length / type spell a DXV3 tag (a 5.8-million-byte legacy payload with type byte 0x44 / 0x59) is read as
  DXV3, as the spec says.
- **New status `DXVK_ERR_BAD_HEADER`** (appended to the enum, so existing values keep their numbers): DXV3
  version byte below 4 or stored flag above 1. `DXVK_ERR_UNSUPPORTED` keeps its meaning ("stored flag on
  an HQ variant") but the decoder no longer returns it; the encoder still does for
  `DXVK_ENC_STORED_PAYLOAD` with `YCG6` / `YG10` (encoder unchanged there). `DXVK_ERR_UNKNOWN_VARIANT`
  is no longer returned by the decoder either: a packet without a DXV3 tag is legacy by definition
  (2.2), and every legacy type byte selects a texture (4.1), so such a packet fails as legacy (typically
  `DXVK_ERR_TRUNCATED` or an LZF error). Both statuses stay for API stability.
- **HQ RGBA in fixed point.** Every exact BC4 palette value is a multiple of 1/35 (eight-value entries are
  sevenths, six-value entries fifths), so the conversion works on integers in units of 1/70 (to hold the
  127.5 midpoint): no floating point, bit-reproducible on every platform. Rounding: floor(v + 1/2). Alpha
  (YG10) is the exact palette value rounded the same way. `DXVK_OPT_PREMULTIPLY` is applied after.
  BC1 / BC3 RGBA is unchanged (spec 8.1 reference convention).
- **Encoder targets in 1/16 units.** Y = (R + 2G + B) / 4 is a multiple of 1/4; the 2 x 2 box average of
  co is a multiple of 1/8 and of cg a multiple of 1/16. A second BC4 encoder takes 16 targets in 1/16
  units and measures error against the exact palette (the decoder the files are played with). Fast:
  endpoints ceil(max) / floor(min), eight-value mode; a flat integer block is one endpoint pair a0 = a1.
  Best: the W3 search (endpoints pulled in by 0..2, a least-squares refit with +-1, the six-value mode near
  0 / 255) on the same metric. The alpha plane (YG10) and every DXT1 / DXT5 block keep the W3 encoder:
  their output is byte-identical to before W6. Chroma targets lie in 0..255 by construction (no clamp
  needed), padding by edge replication as before.
- **Version.** `DXVKIT_VERSION` "1.0.0" plus `DXVK_VERSION_MAJOR/MINOR/PATCH` 1 / 0 / 0 in `dxvkit.h`,
  `dxvk_version_string()`, `DXVKitVersion` / `DXVKitLibraryVersion` in Swift, `dxvk::version()` in C++, `project(VERSION 1.0.0)` in CMake.
- **Vectors.** The 33 W5 forms (256 x 256, 8 frames, our testsrc2 + alpha ramp, every file < 1 MB) are
  copied into `test_vectors/streams/arena/`. Rows: every packet of the forms Arena plays (`decode`, with
  block-stream hashes that must equal the hashes of the form's compressed control: forms are byte edits
  of the same blocks); the forms Arena plays black (`reject`, every packet); the forms Arena does not
  load (`reject`, frame 0: the file is refused). `test_vectors/arena_forms.tsv` records Arena's verdict
  per file (play / black / not a clip) and `dxvtool arena` compares DXVKit's verdict with it.
  `bad_dxdi-no-texture-flag` becomes `synthetic-dxdi_type03-implied-dxt1_pat64`, a `decode` vector.
  HQ RGBA references are regenerated with the 8.4 convention; the old ones stay in git history only.
