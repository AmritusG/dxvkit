# DXVKit encoder design (Phase B, W3, clean room)

Written by the W3 clean-room session from `docs/spec/DXV_FORMAT.md`, `docs/spec/ENCODING_NOTES.md`,
`docs/design.md`, `docs/cleanroom/QUESTIONS.md`, the W1 / W2 handoffs, DXVKit's own decoder (`src/`),
public BC1 / BC3 / BC4 / BC5 (S3TC / RGTC) knowledge, the public FSE / tANS descriptions (Yann Collet,
RFC 8878 section 4.1) and the QuickTime file format. No other DXV implementation was read. Spec sections
are cited as "spec 7.3"; encoding notes as "notes 6".

## 1. What the spec obliges an encoder to emit (extract)

| Area | Obligation (source) |
|---|---|
| Container | One QuickTime video track, sample-entry FourCC `DXD3` for all four DXV3 variants; entry of 86 bytes: data reference index 1, version / revision 0, vendor `FFMP`, temporal / spatial quality 512, width / height = **display** size, 72 dpi (0x00480000) both, data size 0, frame count 1, empty compressor name, depth 24 (also with alpha), colour table id -1, no extension atoms (spec 2.2, notes 1). Track header size = display size. One sample per frame, every sample a sync sample, constant frame rate (one `stts` entry). No variant mixing. |
| Coded size | CW = ceil16(W), CH = ceil16(H) for every variant; every block of the padded texture is in the stream; blocks row by row, top first (spec 2.3). Chroma planes CW/2 x CH/2 (CW/8 x CH/8 blocks). Brief: pad by **edge replication**. |
| Packet header | 12 bytes: tag reversed (`1TXD`, `5TXD`, `6GCY`, `01GY`), byte 4 = 4, byte 5 = 0, byte 6 (stored flag) = 0, byte 7 = 0, u32 payload length; sample size = 12 + payload length exactly (spec 3.1, notes 2). |
| Stored flag | Legal for DXT1 / DXT5 only (payload = raw block stream); never seen in real files; undefined for HQ (decoder must reject). Only behind an explicit option, DXT1 / DXT5 only (spec 3.3). |
| DXT1 layer | Block 0 verbatim (8 bytes). Then per block a 2-bit block flag: 1 = previous block, 2 = u8 + 2 back, 3 = u16 + 258 back (max 65,793), 0 = per-half: a flag for the colour half (bytes 0..3) then one for the index half (bytes 4..7), each copy-with-distance or 4 literal bytes. Flags packed 16 per LE u32, LSB first; a word sits in the stream where the decoder needs its first flag (after the extra bytes of the previous flag). Payload ends at the last byte needed; unused flags of the last word are zero (spec 5, notes 6). |
| DXT5 layer | Block 0 verbatim (16 bytes). Per block: alpha half (bytes 0..7) via an alpha command unless an alpha run is active: 0 = block run of n = u8 + 1 whole blocks equal to their predecessor (256 => u16 extension, 6.4), no colour flags for them; 1 = alpha run (u8 r, 255 => extension): this and the next r alpha halves copy the previous one; 2 = alpha half from u16 + 2 back; 3 = 8 literal bytes. Then the colour half (bytes 8..15): block flag with distance, or 0 + two 4-byte quarters each with distance or literal (spec 6). |
| Count extension | First byte at maximum, then u16 values added while each is 0xFFFF; an exact multiple of 0xFFFF needs a trailing u16 0 (spec 6.4, notes 6). |
| HQ layout | YCG6: one-channel section (Y, BC4) then two-channel section (Co, Cg); YG10: two-channel (Y, A) then (Co, Cg). Section = u32 opcode-area offset, u32 count per channel, literal area (exactly up to the offset), then the opcode stream of channel 0, then channel 1. Section 2 starts right after section 1's last opcode stream; the payload ends right after section 2's last stream (spec 7.1, 7.2, notes 7.1). |
| HQ opcodes | Per channel: two 256-slot dictionaries (E = bytes 0..1, I = bytes 2..4 only), slot h(v) = (v x 0x9E3779B1 mod 2^32) >> 24, inserts exactly as spec 7.4.3's table; block 0 of each channel literal (first 8 / 16 literal bytes) + insert E, I; then per block position, channel 0 then channel 1, one opcode unless the channel's repeat counter is running. 18 opcodes (spec 7.4.3 table). Literal reads come from one cursor shared by both channels, in decode order. Opcode count = opcodes actually emitted (no dangling opcode; must be <= blocks) (spec 7.4, notes 7.2). |
| Opcode streams | Low 2 bits of the first byte: 0 = stored (1 + count bytes), 1 = fill (2 bytes), 2 = FSE. FSE: counts summing to 1024, written LSB first from bit 2 of a u32 then u16 units, width 10 decreasing by one whenever the remainder drops below 2^(width-1); table length = 4 + 2 ceil(max(0, T - 32) / 16); then the bitstream area: u32 L then bits, read from bit L - 1 downwards, MSB first per value; initial 10-bit state, then one transition read per opcode **including after the last one**; L = 32 + bits consumed. Decoding table: spread step **641** (QUESTIONS 2), table log 10, per state x from count(s) upward, bits = 10 - floor(log2 x), baseline = x 2^bits - 1024 (spec 7.3). A single-symbol stream cannot be FSE-coded (a count of 1024 does not fit the first 10-bit field): use fill. Selector 3 is never written (QUESTIONS 3). |
| BC1 (DXT1) | Opaque. Three-colour mode allowed (c0 <= c1) but **never index 3** in it (notes 4). |
| BC3 (DXT5) | Colour half always decoded four-colour (spec 8.2). Alpha half = BC4 rules. |
| BC4 | Unsigned; a0 > a1: 8 values (truncating /7); else 6 values (truncating /5) + 0 + 255 (spec 8.3). |
| YCoCg | Decoder inverse fixed: R = Y + co - cg, G = Y + cg, B = Y - co - cg, co = Co - 128, cg = Cg - 128 (W3 spec text; 127.5 since W5 / W6, see section 4), chroma nearest-neighbour at (x/2, y/2) (spec 8.4). Exact forward: Y = (R + 2G + B) / 4, co = (R - B) / 2, cg = (2G - R - B) / 4; rounding is ours; keep Co, Cg in 0..255 (notes 5). |
| Alpha | The file does not say straight or premultiplied (spec 8.5). Alley writes straight. |
| Self-check | Decode every packet, compare block streams with what was packed (identical), payload / literals / opcodes consumed exactly, every FSE stream leaves 32 bits (notes 8). |

## 2. Architecture

```
include/dxvkit.h   + encoder API (dxvk_encoder_*), dxvk_encode_options, MOV writer (dxvk_mov_writer_*)
src/enc_bc.c       BC1 / BC3 colour / BC4 block encoders, solid-colour tables, RGBA -> planes (+ YCoCg)
src/enc_pack.c     DXT1 / DXT5 block-copy packers, DXV3 header, stored packets
src/enc_hq.c       HQ opcode chooser (mirrors the decoder dictionaries), sections, FSE / stored / fill
src/enc.c          encoder object: allocation at create, dispatch, jobs
src/movwrite.c     QuickTime writer (DXD3 video track only)
include/dxvkit.hpp + dxvk::Encoder, dxvk::MovWriter
swift/             + DXVEncoder, DXVEncodeOptions, DXVMovieWriter
tools/dxvtool.c    + encode, psnr, roundtrip
tools/fuzz_encode.c  libFuzzer: random size / pixels / options -> encode -> decode -> must be exact
```

Principles, same as the decoder: C99, libc only, no globals (all tables live in the encoder object),
**allocation only in `dxvk_encoder_create`** (planes, hash tables, literal / opcode scratch sized for the
frame size), caller-provided packet buffer (`dxvk_encode_bound` gives the worst case), parallelism only
through the caller's `dxvk_jobs` (BCn compression in row bands; the two HQ sections; output is
byte-identical with or without jobs). One encoder object per thread.

## 3. API shape

- `dxvk_encoder_create(variant, width, height, &options, &enc)` / `dxvk_encoder_destroy`.
- `dxvk_encoder_compress(enc, rgba, stride, planes, caps)`: RGBA8 -> block planes in the decoder's
  layout (`dxvk_layout`: BC1 | BC3 | BC4 Y + BC5 CoCg | BC5 YA + BC5 CoCg), padded by edge replication.
- `dxvk_encoder_pack(enc, planes, packet, cap, &size)`: the blocks-in path (any BCn encoder's planes ->
  DXV3 packet), lossless.
- `dxvk_encoder_encode(enc, rgba, stride, packet, cap, &size)`: compress + pack.
- `dxvk_encode_bound(variant, w, h)`: the largest packet the encoder can emit.
- Options: `effort` (fast / best), `opcodes` (auto = cheapest of FSE / stored / fill per stream; stored =
  stored / fill only), flags `DXVK_ENC_PREMULTIPLIED_INPUT` (the input colour is premultiplied and is
  written as such; without it the input is straight and written straight; the encoder never converts),
  `DXVK_ENC_STORED_PAYLOAD` (DXT1 / DXT5 only: stored-flag packet, never seen in real files), `jobs`.
- MOV writer: `dxvk_mov_writer_open_file(path, w, h, timescale, frame_duration, &mw)` (or a caller
  `write_at` sink), `dxvk_mov_writer_add(mw, packet, size)`, `dxvk_mov_writer_finish(mw)` (writes `moov`,
  patches `mdat`, chooses `stco` or `co64`), `dxvk_mov_writer_close`.

## 4. Block encoders (from the public BCn definitions)

The error metric is the squared error of the values **our decoder produces** (spec 8 palettes: bit
replication, truncating integer interpolation). GPU decoders differ by at most about 1 per value.

| Effort | BC1 colour (DXT1, DXT5 colour) | BC4 (Y, A, Co, Cg, DXT5 alpha) |
|---|---|---|
| fast | principal axis (power iteration) -> extreme projections -> one least-squares refit -> 565 rounding -> exact nearest-palette indices; four-colour only; solid blocks from optimal single-colour tables | min / max endpoints, eight-value mode, exact nearest indices |
| best | as fast, then iterated least-squares refits, a +-1 search on each 565 endpoint component, DXT1 also tries the three-colour mode (without index 3) | eight-value mode with an endpoint search around min / max plus a least-squares refit, and the six-value mode (0 / 255 extremes) when the block touches 0 or 255; keeps the lowest error |

YCoCg (YCG6, YG10), W3: Y = (R + 2G + B + 2) >> 2 per pixel; Co, Cg = the mean of the 2 x 2 pixels' exact
co / cg, rounded, + 128, clamped to 0..255. **Since W6 (1.0.0, finding F7):** the exact unrounded transform
of ENCODING_NOTES 5, like Alley: Y = (R + 2G + B) / 4, Co / Cg = the 2 x 2 box average of (R - B) / 2 and
(2G - R - B) / 4, + 127.5, held in 1/16 units and handed to a BC4 encoder that measures error against the
exact palette (`dxvk_bc4_encode_fine`; same search as the W3 BC4 encoder per effort). The alpha plane and
DXT1 / DXT5 are unchanged. Same transform for both efforts.

## 5. Packers

- DXT1 / DXT5: hash tables (most recent block index per value: whole block, each half / quarter at its
  own position, DXT5 alpha half); a hit is verified byte for byte; the shortest coding wins (the nearest
  occurrence is always the cheapest). Whole-block copy versus per-half / per-quarter flags: the cheaper in
  bits. DXT5 alpha: block run when the block equals its predecessor, alpha run when only the alpha half
  does, then a u16 + 2 copy, then a literal. Best effort: bigger tables and an alpha run that stops
  before a long whole-block run.
- HQ: per block, a run (opcode 0 for >= 4 identical-to-previous blocks, else opcode 1), else the
  cheapest of opcodes 2..17 by (8 x literal bytes + estimated opcode bits). Fast: fixed opcode-cost
  prior, one pass. Best: two passes per section, the second with opcode costs from the first pass's
  histogram. Dictionaries updated exactly like the decoder after every emitted block.
- Opcode streams: FSE with counts normalised to 1024 (every used opcode >= 1), encoded in reverse with
  the tANS encoder derived from the decoding table (for a next state v and symbol s: X = v + 1024, the
  bit count n makes X >> n fall in [count(s), 2 count(s)), the n low bits of X are written, the new state
  is the state holding (s, X >> n)); compared byte-exactly with stored and fill; the smallest wins
  (`opcodes = auto`).

## 6. QuickTime writer

`ftyp` (`qt  `), `wide`, `mdat` (patched at finish; turned into a 64-bit `mdat` over the `wide` atom when
larger than 4 GB), then `moov`: `mvhd`, one `trak` (`tkhd` enabled / in movie / in preview, display size;
`mdia`: `mdhd` timescale + duration, `hdlr` mhlr/vide, `minf`: `vmhd`, `hdlr` dhlr/alis, `dinf`/`dref`
alis self-reference, `stbl`: `stsd` with the 86-byte DXD3 entry, `stts` one entry, `stsc` one entry (one
sample per chunk), `stsz` per-sample sizes, `stco` or `co64`). No `stss` (every sample is a sync sample).
Audio is the caller's job.

## 7. Verification plan (brief W3)

Round trip (variants x efforts x opcode modes x sizes 16x16, 18x10, 333x221, 1920x1080, 908x2276,
3840x2160, 1x1): compress -> pack -> our decoder -> planes identical, consumption exact. ffmpeg / ffprobe
binaries as a black-box decoder; Arena loading the files (no triggering); Alley comparison on the
`resolume` vectors' testsrc2 source; speed; ASan + UBSan; 30-minute libFuzzer run on the encoder;
decoder vectors unchanged (127 / 91 / 11).
