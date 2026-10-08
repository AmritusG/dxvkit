# DXVKit

A clean-room, MIT-licensed decoder and encoder for Resolume's DXV video codec: legacy DXV1/DXV2 (`DXDI`,
LZF and stored; decode only), DXV3 DXT1, DXT5, YCG6 "High Quality" and YG10 "High Quality With Alpha",
including the FSE-coded opcode streams Resolume writes.

Status: **decoder (W2) and encoder (W3) complete.** C99 core (libc only), header-only C++ wrapper,
SwiftPM package, `dxvtool`. Decoder: every test vector matches byte for byte (127/127 block streams,
91/91 RGBA, 11/11 invalid packets rejected). Encoder: RGBA8 or caller block planes -> DXV3 packets for
all four variants, two effort levels, a QuickTime writer; every encoded packet decodes back to exactly
the packed block streams. Written from the specification alone; see `CLAUDE.md` for the clean-room
rules and `docs/cleanroom/` for provenance.

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

Encoding (C):

```c
dxvk_encode_options eo = {0, DXVK_EFFORT_BEST, DXVK_OPCODES_AUTO, NULL};  /* or NULL = fast */
dxvk_encoder *enc;
dxvk_encoder_create(DXVK_VARIANT_YCG6, w, h, &eo, &enc);  /* all allocation happens here */
size_t cap = dxvk_encode_bound(DXVK_VARIANT_YCG6, w, h), n;
dxvk_encoder_encode(enc, rgba, stride, packet, cap, &n);    /* RGBA8 -> one DXV3 packet */
dxvk_encoder_pack(enc, planes, packet, cap, &n);            /* or: your own BCn planes -> packet */
dxvk_mov_writer *mw;                                        /* .mov with one DXD3 video track */
dxvk_mov_writer_open_file("out.mov", w, h, 30000, 1001, &mw);
dxvk_mov_writer_add(mw, packet, n);  /* per frame */
dxvk_mov_writer_finish(mw); dxvk_mov_writer_close(mw);
```

Effort `FAST` (live / batch) and `BEST` (archive) change the BCn endpoint search and the match /
opcode search (see `include/dxvkit.h`). Odd sizes are padded to 16 x 16 by edge replication. Input is
straight alpha by default (`DXVK_ENC_PREMULTIPLIED_INPUT` records premultiplied input; the bytes are
the same). Design: `docs/design-encoder.md`.

C++ (`include/dxvkit.hpp`): `dxvk::Movie`, `dxvk::Frame`, `dxvk::Encoder`, `dxvk::MovWriter` (RAII,
no exceptions).
Swift (`Package.swift`, module `DXVKit`): `DXVMovie`, `DXVFrame`, `DXVLayout`, `DXVOptions`,
`DXVEncoder`, `DXVEncodeOptions`, `DXVMovieWriter`.

## Build

```sh
cmake -S . -B build && cmake --build build && (cd build && ctest)   # universal arm64 + x86_64 on macOS
build/dxvtool verify test_vectors/vectors.tsv
swift test
```

`dxvtool`: `info`, `dump-blocks`, `dump-rgba`, `dump-samples`, `verify`, `check` (decode every
packet, report byte consumption), `bench`; encoder side: `encode` (PNG / PAM / PPM files or raw RGBA,
e.g. `ffmpeg ... -f rawvideo -pix_fmt rgba - | dxvtool encode - out.mov --size 1920x1080 --variant
ycg6 --effort best`), `bench-encode`, `repack` (re-pack another file's block streams with DXVKit's
packer), `psnr`, `cmp-rgba`. `tests/encoder_test.c`: the encoder round trip (ctest).
`tools/oracle.sh`: compares encoder output with the ffmpeg binary used as a black-box decoder. Sanitizers: `-DDXVK_SANITIZE=ON`; fuzzers: `-DDXVK_FUZZ=ON` with a clang that
ships libFuzzer (e.g. Homebrew LLVM).

## Documents

- Specification: `docs/spec/DXV_FORMAT.md`; encoder notes: `docs/spec/ENCODING_NOTES.md`
- Design: `docs/design.md` (decoder), `docs/design-encoder.md` (encoder); clean-room questions:
  `docs/cleanroom/QUESTIONS.md`
- Test vectors: `test_vectors/README.md`; provenance: `docs/cleanroom/PROVENANCE.md`

Licence: MIT (`LICENSE`). The library uses libc only; `dxvtool` links the system zlib (zlib licence) for PNG input.
