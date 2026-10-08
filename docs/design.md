# DXVKit design (Phase B, W2)

Written by the clean-room session from `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md`,
`test_vectors/` and public container / BCn / FSE documentation only. Spec section numbers are given as
"spec 7.3".

## What the spec asks of a decoder (extract)

| Area | Requirement (spec section) |
|---|---|
| Container | QuickTime; one DXV video track, sample-entry FourCC `DXD3` (DXV3) or `DXDI` (legacy); entry width / height = display size (2.2). Variant is in every packet, not in the container. |
| Coded size | CW = ceil16(W), CH = ceil16(H), every variant (2.3). Chroma planes CW/2 x CH/2. |
| DXV3 header | 12 bytes: tag (`DXT1`/`DXT5`/`YCG6`/`YG10` as LE u32 = BE FourCC value), version, minor, stored flag, reserved, u32 payload length (3.1). Reject: < 12 bytes, unknown tag, length + 12 > packet (3.2). Accept trailing bytes. |
| Stored flag | DXT1 / DXT5: payload is the block stream, at least as long as needed (3.3). HQ + stored: reject (SHOULD). |
| Legacy | u32 type word: bits 0..23 length, 24..27 version nibble, 0x40 top-byte DXT5, else 0x20 or nibble 2 DXT1, else reject; 0x80 = stored, else LZF; LZF output length must equal the texture exactly (4). |
| DXT1 | block-copy layer, 2-bit flags packed 16 per u32, distances 1 / u8+2 / u16+258 in blocks; halves of 4 bytes (5). |
| DXT5 | same flags; alpha commands (block run, alpha run, u16+2 back, literal); colour half in quarters; count extension (6). |
| HQ | two plane sections, each: op-area offset, opcode count(s), literal area, opcode stream(s) stored / fill / FSE (table log 10, spread step 641, bitstream read downwards from bit L-1); 18 opcodes over BC4 blocks with two 256-slot hashed dictionaries per channel (7). |
| Pixels | BC1 bit-replicated 565, truncating /3 and /2 palettes, DXT1 opaque (alpha 255, index 3 black); BC3 alpha / BC4 palettes; BC3 colour always four-colour; YCoCg plain inverse with nearest-neighbour chroma (8). Alpha values untouched (8.5). |
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
- Accepted sample-entry FourCCs: `DXD3`, `DXDI` only (the spec's MAY for other FourCCs is not taken:
  `DXT1` / `DXT5` as a container FourCC could belong to another codec).
