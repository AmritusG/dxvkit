# DXVKit

A clean-room, MIT-licensed decoder for Resolume's DXV video codec: legacy DXV1/DXV2 (`DXDI`, LZF and
stored), DXV3 DXT1, DXT5, YCG6 "High Quality" and YG10 "High Quality With Alpha", including the
FSE-coded opcode streams Resolume writes.

Status: **Phase B (W2) decoder complete.** C99 core (libc only), header-only C++ wrapper, SwiftPM
package, `dxvtool`. Every test vector matches byte for byte (127/127 block streams, 91/91 RGBA, 11/11
invalid packets rejected). The encoder is a later wave. Written from the specification alone; see
`CLAUDE.md` for the clean-room rules and `docs/cleanroom/` for provenance.

## Use

C (`include/dxvkit.h`):

```c
dxvk_mov *mov;
dxvk_mov_open_file("clip.mov", &mov);
const dxvk_mov_info *info = dxvk_mov_get_info(mov);       /* size, frame count, rate, generation */
dxvk_mov_read_packet(mov, n, packet, cap, &size);           /* one frame's bytes */
dxvk_decode_blocks(info->generation, packet, size, info->width, info->height,
                   &options, planes, capacities, &layout, NULL);  /* BCn block streams (GPU path) */
dxvk_blocks_to_rgba(&layout, planes, &options, rgba, stride);     /* optional CPU path */
```

Output planes (whole coded texture = display size rounded up to 16 on both axes, rows top first):

| Variant | plane 0 | plane 1 |
|---|---|---|
| DXT1, legacy DXT1 | BC1 (opaque) | - |
| DXT5, legacy DXT5 | BC3 | - |
| YCG6 | BC4: Y | BC5 layout, half size: Co, Cg |
| YG10 | BC5 layout: Y, alpha | BC5 layout, half size: Co, Cg |

YCoCg to RGB: R = Y - cg + co, G = Y + cg, B = Y - cg - co with co = Co - 128, cg = Cg - 128.
Alpha is returned as stored (Alley writes straight colour); `DXVK_OPT_PREMULTIPLY` converts for RGBA.
No allocation and no threads in the decode path; `dxvk_jobs` lets the caller run the two HQ sections
and the RGBA bands in parallel.

C++ (`include/dxvkit.hpp`): `dxvk::Movie`, `dxvk::Frame` (RAII, no exceptions).
Swift (`Package.swift`, module `DXVKit`): `DXVMovie`, `DXVFrame`, `DXVLayout`, `DXVOptions`.

## Build

```sh
cmake -S . -B build && cmake --build build && (cd build && ctest)   # universal arm64 + x86_64 on macOS
build/dxvtool verify test_vectors/vectors.tsv
swift test
```

`dxvtool`: `info`, `dump-blocks`, `dump-rgba`, `verify`, `check` (decode every packet, report byte
consumption), `bench`. Sanitizers: `-DDXVK_SANITIZE=ON`; fuzzers: `-DDXVK_FUZZ=ON` with a clang that
ships libFuzzer (e.g. Homebrew LLVM).

## Documents

- Specification: `docs/spec/DXV_FORMAT.md`; encoder notes: `docs/spec/ENCODING_NOTES.md`
- Design: `docs/design.md`; clean-room questions: `docs/cleanroom/QUESTIONS.md`
- Test vectors: `test_vectors/README.md`; provenance: `docs/cleanroom/PROVENANCE.md`

Licence: MIT (`LICENSE`).
