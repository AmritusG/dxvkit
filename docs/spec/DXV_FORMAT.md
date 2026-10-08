# DXV video format: specification

Status: Phase A, W1 (2026-10-08). Written by the dirty-room session (see `docs/cleanroom/PROVENANCE.md`).
This document describes the DXV format and the behaviour a decoder must have. It contains no source code
and no pseudocode. Everything marked **measured** was checked on real files and on the test vectors in
`test_vectors/`; everything marked **unobserved** is described for completeness but never appeared in a
real file on the reference machine.

Revised in W4 (2026-10-08, Phase A, dirty room; `docs/handoffs/dxvkit_2026-10-08_w4-spec-cleanup-results.md`):
2.3 (wording), 7.3 (selector 3, the codings Resolume uses, single-symbol tables), 7.5 (the extra Y opcode
Resolume writes, and what is legal), 8.2 (colour endpoint order), 8.5 (alpha convention by writer and
reader), section 10.

Revised in W5 (2026-10-08, Phase A, dirty room; `docs/handoffs/dxvkit_2026-10-08_w5-close-open-results.md`):
2.2 (FourCCs), 3.1-3.3 (version byte, stored flag, stored HQ layout), 4.1 (legacy texture selection),
8.1 (FFmpeg's BC1 convention), 8.2 (FFmpeg reads BC3 colour as four-colour; W4 wording corrected), 8.4
(chroma midpoint 127.5, nearest chroma, out-of-gamut handling), 9, 10 (nothing left open). The authority
for each forms question is **Resolume Arena 7.28 as a black box** ("Arena 7.28 behaviour"): synthetic
files of each form were loaded into Arena, their thumbnails read, and each clip fired by Amrit; for
the HQ colour questions Arena's composition output was captured pixel-exact (Syphon) and Alley 7.28's
encodes of a known lossless source were measured.

Revised in W7 (2026-10-08, Phase A; `docs/handoffs/dxvkit_2026-10-08_w7-docs-close-results.md`): 4.1
and 8.4 no longer describe DXVKit's pre-1.0.0 state (QUESTIONS W6-1).

Conventions: all multi-byte integers inside DXV packets are **little-endian** unless stated otherwise
(QuickTime container fields are big-endian, as always in QuickTime). `u8`, `u16`, `u32` = unsigned
integers of 1, 2, 4 bytes. Bit 0 = least significant bit. "Block" = one 4x4-pixel compressed block.
`ceil16(n)` = n rounded up to a multiple of 16. MUST / SHOULD / MAY as in RFC 2119.

---

## 1. Overview

DXV is a GPU-texture video codec. Every frame is stored as one or two arrays of standard block-compressed
texture blocks (BC1 / BC3 / BC4), wrapped in a lossless byte-level compression layer. A decoder:

1. reads one packet (one QuickTime sample = one complete frame, no inter-frame dependency);
2. reads the packet header, which names the variant;
3. undoes the lossless layer, producing the block stream(s);
4. uploads the block stream(s) to the GPU, or converts them to pixels on the CPU.

There are two generations and five texture layouts:

| Generation | Container FourCC | Variant | Block stream(s) after the lossless layer | Alpha |
|---|---|---|---|---|
| Legacy (DXV1/DXV2) | `DXDI` | legacy DXT1 | one BC1 stream | no |
| Legacy | `DXDI` | legacy DXT5 (unobserved) | one BC3 stream | yes |
| DXV3 "Normal Quality" | `DXD3` | `DXT1` | one BC1 stream | no |
| DXV3 "Normal Quality With Alpha" | `DXD3` | `DXT5` | one BC3 stream | yes |
| DXV3 "High Quality" | `DXD3` | `YCG6` | luma: one BC4 stream; chroma: one two-channel stream (Co, Cg) at half resolution | no |
| DXV3 "High Quality With Alpha" | `DXD3` | `YG10` | luma+alpha: one two-channel stream (Y, A); chroma: one two-channel stream (Co, Cg) at half resolution | yes |

A "two-channel stream" is a sequence of 16-byte blocks, each made of two 8-byte BC4 blocks (the same byte
layout as BC5): bytes 0..7 = channel 0, bytes 8..15 = channel 1.

The lossless layers:

| Variant | Lossless layer | Section |
|---|---|---|
| legacy DXT1 / DXT5 | stored, or LZF | 4 |
| `DXT1` | 32-bit-element back-reference coder ("block-copy layer") | 5 |
| `DXT5` | the same family, with an extra command per block for the alpha half | 6 |
| `YCG6`, `YG10` | per-block opcode coder over BC4 blocks with two dictionaries; the opcodes are themselves stored, filled, or entropy-coded with FSE (tANS) | 7 |
| any DXV3 variant | "stored" flag: the payload is the block stream itself | 3.3 |

---

## 2. Container

### 2.1 File

DXV lives in a QuickTime (`.mov`) file. **Measured:** the DXV files found (695) carry one DXV video track; some
also carry audio. The video track's sample description (`stsd`) has one entry. The track is
constant frame rate in practice (`stts` usually has one entry); every sample is a key frame and decodes
on its own.

### 2.2 Sample description entry

The entry is a standard QuickTime visual sample entry (big-endian fields):

| Offset in entry | Size | Field | Value written by Resolume (measured) |
|---|---|---|---|
| 0 | 4 | entry size | 86 (no extension atoms) |
| 4 | 4 | data format (FourCC) | `DXD3` for every DXV3 variant; `DXDI` for legacy |
| 8 | 6 | reserved | 0 |
| 14 | 2 | data reference index | 1 |
| 16 | 2 | version | 0 |
| 18 | 2 | revision | 0 |
| 20 | 4 | vendor | `FFMP` |
| 24 | 4 | temporal quality | 512 |
| 28 | 4 | spatial quality | 512 |
| 32 | 2 | width | display width in pixels |
| 34 | 2 | height | display height in pixels |
| 36 | 4 | horizontal resolution | 72.0 (16.16 fixed) |
| 40 | 4 | vertical resolution | 72.0 |
| 44 | 4 | data size | 0 |
| 48 | 2 | frame count | 1 |
| 50 | 32 | compressor name (Pascal string) | empty |
| 82 | 2 | depth | 24, also for the alpha variants |
| 84 | 2 | colour table id | -1 |

Other writers differ only in harmless fields (FFmpeg: compressor name "Lavc… dxv" plus `fiel` and `pasp`
atoms; the legacy file found: compressor name "DXV", depth 32, `fiel` and `gama` atoms). A decoder MUST
NOT depend on vendor, quality, depth, compressor name or extension atoms. The **variant is not in the
container**: it is signalled in every packet (section 3). **Measured:** no file mixes variants between
packets; a decoder MAY determine the variant from the first packet and SHOULD still check every packet.

The sample entry's width and height are the **display size**. The coded texture size is derived from it
(2.3).

FourCCs other than `DXD3` and `DXDI` are **unobserved** in real files. **Arena 7.28 behaviour** (W5,
256 x 256 files whose packets are valid DXV3 `DXT1` / `YCG6` or legacy DXT1, only the FourCC changed):

| Sample-entry FourCC | Arena 7.28 |
|---|---|
| `DXD3`, `DXDI` | loads and plays |
| `DXD4` | loads and plays (reported as "DXV 3.0") |
| `DXV3`, `DXT1`, `YCG6`, `DXD1`, `DXDA` | not loaded as video (no clip appears; log: "Could not load or generate thumbnails") |
| `DXDI` holding DXV3 packets; `DXD3` holding legacy packets | loads and plays: the packet decides the generation, not the FourCC |

Rule: a decoder SHOULD accept the FourCCs `DXD3`, `DXDI` and `DXD4` and MUST NOT accept others as DXV. It
MUST NOT use the FourCC to choose between the legacy and DXV3 packet formats; it decides per packet: a
packet whose first four bytes are one of the four DXV3 texture tags (3.1) is a DXV3 packet, any other
packet is legacy (4.1). How Arena itself tells them apart was not observed beyond this. A legacy packet
could only be mistaken for DXV3 if its 24-bit length were exactly the value the tag's first three bytes
spell (several million bytes) and its type byte the tag's fourth byte; a writer SHOULD avoid that
legacy length.

### 2.3 Coded size (padding)

**Every variant, both generations, codes a texture whose width AND height are rounded up to a multiple
of 16:** coded width `CW = ceil16(W)`, coded height `CH = ceil16(H)`, where W x H is the display size.

**Measured** (by finding the only texture size whose decoding consumes the packet exactly, with every
opcode used and every literal byte read; for Resolume `YCG6`, exactly apart from the one extra Y opcode
described in 7.5):

| File (writer) | Display | Size that decodes exactly |
|---|---|---|
| Resolume DXT1 | 908 x 2276 | 912 x 2288 |
| Resolume DXT1 | 1920 x 1080 | 1920 x 1088 |
| Resolume DXT1 | 1920 x 818 | 1920 x 832 |
| Resolume DXT5 | 912 x 2276 | 912 x 2288 |
| Resolume YCG6 | 908 x 2276 | 912 x 2288 |
| Resolume YCG6 / YG10 | 1920 x 1080 | 1920 x 1088 |
| legacy DXT1 (`DXDI`) | 1920 x 1080 | 1920 x 1088 (LZF output is exactly 1,044,480 bytes) |
| GlEnc DXT1 | 18 x 10 / 100 x 36 | 32 x 16 / 112 x 48 |
| GlEnc YCG6 | 18 x 10 | 32 x 16 |
| GlEnc YCG6 and YG10 | 1284 x 724 | 1296 x 736 |

(The brief this spec was written against expected "DXV3 pads the width only"; that is wrong for every
DXV3 file found. Decoders that size the texture with only 4-row height alignment still display correctly
for most sizes, because the decoded rows they skip are padding at the bottom, but they then need extra
tolerances elsewhere; see 7.5.)

Block counts:

| Plane | Size in pixels | Blocks per row | Block rows |
|---|---|---|---|
| BC1 / BC3 texture, Y, Y+A | CW x CH | CW / 4 | CH / 4 |
| Co+Cg chroma (YCG6, YG10) | CW/2 x CH/2 | CW / 8 | CH / 8 |

Blocks are stored row by row, left to right, top to bottom (top row of the picture first). The padding
area holds whatever the encoder put there; Resolume fills it with flat content. A decoder MUST decode the
whole coded texture and MUST present only the top-left W x H pixels.

Minimum size: width and height of at least 1 pixel (coded 16 x 16). **Unobserved:** sizes above 16384.
The legacy payload length field is 24 bits, which limits a legacy packet to 16,777,215 payload bytes.

---

## 3. DXV3 packet header (`DXD3`)

### 3.1 Layout

Every DXV3 packet begins with 12 bytes:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | texture tag | four ASCII bytes, stored reversed: `1TXD` = DXT1, `5TXD` = DXT5, `6GCY` = YCG6, `01GY` = YG10 (read as a little-endian u32 they equal the big-endian FourCC values of `DXT1`, `DXT5`, `YCG6`, `YG10`) |
| 4 | 1 | format version | version + 1. **Measured:** always 4 (version 3). Values below 4 are invalid (3.2) |
| 5 | 1 | minor version | **measured:** always 0; any value is accepted |
| 6 | 1 | stored flag | 0 = the payload is compressed; 1 = the payload IS the block stream (3.3); any other value is invalid (3.2). **Measured:** always 0 in real files |
| 7 | 1 | reserved | **measured:** always 0; ignore |
| 8 | 4 | payload length | u32: number of payload bytes that follow the header |

The payload is bytes 12 .. 12 + payload length.

**Measured:** in all 397,117 DXV3 packets found, payload length + 12 equals the QuickTime sample size
exactly, version byte = 4, minor = 0, stored flag = 0, reserved = 0.

### 3.2 Validation

A decoder MUST reject a packet when:
- it is shorter than 12 bytes;
- the texture tag is not one of the four values above;
- payload length + 12 > packet size.

A decoder SHOULD accept trailing bytes after the payload (payload length + 12 < packet size) and ignore
them; none were observed.

**Arena 7.28 behaviour** for the other header bytes (W5; 256 x 256 `DXT1` packets from DXVKit's encoder,
one byte changed; Arena shows the version it parsed as "DXV v.m" with v = version byte - 1, m = minor):

| Header bytes | Arena 7.28 | Rule |
|---|---|---|
| version byte 4 (control), 5, 6 (also 5 on `YCG6`) | plays correctly ("DXV 3.0", "4.0", "5.0") | accept |
| version byte 3, 0 | loads, plays **black** ("DXV 2.0", "DXV -1.0") | MUST reject |
| minor byte 1 | plays correctly ("DXV 3.1") | accept any value |
| reserved byte 1 | plays correctly | ignore |
| stored flag 2, with a stored payload or with a compressed one | loads, plays **black** | MUST reject |

So: a decoder MUST reject a version byte below 4 and a stored flag other than 0 or 1; it MUST accept
any version byte of 4 or more and any minor and reserved value, and MAY log unusual ones.

### 3.3 Stored payloads

When the stored flag is 1 the payload is the uncompressed result, exactly as long as the coded texture
needs (2.3): for `DXT1` the BC1 stream, for `DXT5` the BC3 stream, for `YCG6` the Y stream (BC4, CW x CH
/ 2 bytes) immediately followed by the chroma stream (16-byte Co+Cg blocks, CW x CH / 4 bytes), for
`YG10` the Y+A stream (16-byte blocks, Y then A, CW x CH bytes) immediately followed by the chroma
stream. These are the same byte layouts as the reference outputs (7.6). The payload MUST be at least
that long (reject otherwise); extra bytes are ignored.

Stored payloads are **unobserved** in real files. **Arena 7.28 behaviour** (W4 and W5): it plays
stored-flag `DXT1` and `DXT5` correctly (W4, Amrit), and stored-flag `YCG6` and `YG10` packets laid out
as above correctly (W5: thumbnails identical to the compressed controls, and Amrit's playback check).
(Test vectors: `synthetic-dxv3_dxt1-rawflag_pat64`, `synthetic-dxv3_dxt5-rawflag_pat64`; the HQ forms
are in the W5 Arena set, `docs/handoffs/dxvkit_2026-10-08_w5-close-open-results.md`.)

---

## 4. Legacy packets (`DXDI`)

### 4.1 Header

A legacy packet begins with one u32 (little-endian), the **type word**:

| Bits | Field |
|---|---|
| 0..23 | payload length (bytes after the 4-byte type word) |
| 24..27 | version nibble (version + 1) |
| 28 | (unused) |
| 29 (0x20 in the top byte) | texture is DXT1 |
| 30 (0x40 in the top byte) | texture is DXT5 |
| 31 (0x80 in the top byte) | payload is stored (uncompressed); clear = LZF |

So the top byte (bits 24..31) is the "type byte". Texture selection:
1. type byte bit 0x40 set: DXT5 (BC3 stream);
2. otherwise: DXT1 (BC1 stream), whatever bit 0x20 and the version nibble say.

The version nibble and bit 28 do not affect decoding.

**Measured:** the only legacy clip found (1920 x 1080, 201 frames) uses type byte 0x23 (DXT1, LZF,
version nibble 3) in every packet. All other type bytes are **unobserved** in real files.

**Arena 7.28 behaviour** (W5; 256 x 256 legacy packets built from DXVKit-encoded block streams, LZF or
stored; Arena shows "DXV v.m" with v = version nibble - 1 for nibbles up to 3 and 2 above, m = 2 when bit
0x20 is set, 0 otherwise; the label has no effect on decoding). Every form below loads and plays correctly:

| Type byte | Payload | Arena 7.28 |
|---|---|---|
| 0x23 (control, the real form) | LZF DXT1 | plays ("DXV 2.2") |
| 0x43 | LZF DXT5 | plays, with alpha ("DXV 2.0") |
| 0xA3, 0xC3 | stored DXT1, stored DXT5 | play |
| 0x02 (nibble 2, no texture bit) | LZF DXT1 | plays as DXT1 ("DXV 1.0") |
| 0x21, 0x22, 0x24, 0x20 (nibbles 1, 2, 4, 0) | LZF DXT1 | play |
| 0x63 (both texture bits) | LZF DXT5 | plays as DXT5 |
| 0x03 (nibble 3, no texture bit) | LZF DXT1 | plays as DXT1 ("DXV 2.0", "No Alpha") |

The W1 rule ("no texture bit and nibble other than 2: reject") is therefore withdrawn: Arena never
rejects a legacy packet for its type byte. A decoder MUST select the texture as above and MUST reject
only: packet shorter than 4 bytes; payload length + 4 > packet size; and the payload errors of 4.2.
(The W1 invalid vector for type byte 0x03 with DXT1 data was reclassified in W6 as the decode vector
`synthetic-dxdi_type03-implied-dxt1_pat64`; DXVKit 1.0.0 follows this rule, W5 finding F1.)

### 4.2 Payload

Stored: the payload is the block stream; it MUST be at least as long as the coded texture needs; extra
bytes are ignored.

LZF: the payload is one LZF stream (Marc Lehmann's liblzf format). Its decompressed length MUST equal the
coded texture's block stream length exactly (BC1: CW x CH / 2 bytes; BC3: CW x CH bytes). The whole
payload is one stream: decoding continues until the last payload byte. LZF in brief (see liblzf
documentation for the authoritative description):

| Control byte c | Meaning | Extra bytes |
|---|---|---|
| c < 32 | literal run: copy the next c + 1 bytes to the output | the c + 1 literal bytes |
| c >= 32, (c >> 5) < 7 | back-reference of length (c >> 5) + 2 | 1 byte `lo` |
| c >= 32, (c >> 5) = 7 | back-reference of length 7 + `len` + 2 | 1 byte `len`, then 1 byte `lo` |

The back-reference source starts ((c & 31) x 256 + `lo`) + 1 bytes before the current end of the output
and is copied byte by byte (overlapping copies repeat data). A decoder MUST reject: a literal run or
back-reference that runs past the end of the payload; a back-reference starting before the output's
beginning; output longer than, or (at the end of the payload) shorter than, the expected length.

Note: one widely used open-source decoder stops LZF decoding while 2 or fewer input bytes remain, which
drops a final 2-byte operation. Correct behaviour is to decode every byte.

---

## 5. DXV3 `DXT1`: the block-copy layer for BC1

### 5.1 Output

CW/4 x CH/4 BC1 blocks of 8 bytes each. Each block is viewed as two 4-byte **halves**: the colour half
(bytes 0..3: the two RGB565 endpoints) and the index half (bytes 4..7: the 2-bit indices).

### 5.2 Flags

The payload is one byte stream. Interleaved with literal bytes it carries **flag words**: u32 values,
each holding 16 two-bit **flags**, used from the least significant pair upwards. A new flag word is read
from the current stream position at the moment a flag is needed and the previous word is used up (16
flags taken). Bytes that belong to a flag (below) are read right after that flag is taken, before
anything else.

A flag selects a **distance** in blocks:

| Flag | Distance | Extra bytes read |
|---|---|---|
| 0 | none: "new data" | none |
| 1 | 1 (the previous block) | none |
| 2 | u8 + 2 (2 .. 257) | one u8 |
| 3 | u16 + 258 (258 .. 65,793) | one u16 |

A distance d means "the same position in the block d blocks back". The distance MUST NOT exceed the
index of the block being written (block 0 is the first block); reject otherwise.

### 5.3 Decoding

1. Block 0 = the first 8 payload bytes, verbatim.
2. For each following block, in order:
   - take one flag (the **block flag**);
   - if it has a distance: the whole block (8 bytes) is a copy of the block that far back;
   - if it is 0: take a flag for the colour half, then (after its extra bytes and, if 0, its 4 literal
     bytes) a flag for the index half. For each half: with a distance, copy that half from the block that
     far back; with 0, read 4 literal bytes as the half.
3. Stop when the last block has been written. Unused flags in the current flag word and any remaining
   payload bytes are ignored. **Measured:** Resolume payloads end exactly at the last byte needed.

A decoder MUST reject a packet whose payload ends before the texture is complete (a flag word, an extra
byte or a literal that is not there).

Byte example (start of a payload): `[8 bytes block 0] [flag word u32] [extras / literals in flag order …]`.

---

## 6. DXV3 `DXT5`: the block-copy layer for BC3

### 6.1 Output

CW/4 x CH/4 BC3 blocks of 16 bytes: the **alpha half** (bytes 0..7: two alpha endpoints and 48 bits of
3-bit indices) and the **colour half** (bytes 8..15, a BC1 colour block). The colour half is further
split into two 4-byte **quarters** (bytes 8..11 endpoints, 12..15 indices).

### 6.2 Flags

The same flag words as in 5.2 (16 two-bit values per u32, least significant first, read when needed).
Distances are counted in 16-byte blocks, with the same table as 5.2 (1; u8 + 2; u16 + 258).

### 6.3 Per-block structure

1. Block 0 = the first 16 payload bytes.
2. A decoder keeps an **alpha-run counter**, initially 0.
3. For each following block, in order:

   **Alpha half.** If the alpha-run counter is above 0: decrement it; the alpha half = the alpha half of
   the previous block (no flag is taken). Otherwise take one flag and treat it as an **alpha command**:

   | Alpha command | Meaning | Extra bytes |
   |---|---|---|
   | 0 | **block run**: n = u8 + 1; if that gives 256, extend (6.4). The next n blocks (starting with the current one) are each a full 16-byte copy of the block before it. Then continue with the next block at "Alpha half" again (no colour half is read for these blocks). If the texture ends first, stop. | u8 (+ extension) |
   | 1 | **alpha run**: r = u8; if r = 255, extend (6.4). The alpha half = the previous block's alpha half, and the alpha-run counter becomes r (so the next r blocks also copy their alpha half from their previous block) | u8 (+ extension) |
   | 2 | the alpha half = the alpha half of the block u16 + 2 blocks back | u16 |
   | 3 | the alpha half = 8 literal bytes | 8 bytes |

   **Colour half** (always, except after a block run). Take a block flag. With a distance: the colour half
   (8 bytes) = the colour half of the block that far back. With 0: take a flag for the first quarter (with
   0: 4 literal bytes), then a flag for the second quarter, exactly like the two halves of 5.3 but on
   quarters; each quarter copies the same quarter from the block that far back.

4. Stop when the last block is written; leftovers are ignored as in 5.3.

### 6.4 Count extension

Used by alpha commands 0 and 1 (and by HQ opcode 0, 7.4). When the first count byte reaches its maximum
(block run: n = 256 after the +1; alpha run and HQ: byte = 255), the decoder reads u16 values and adds
each one to the count, continuing while the u16 just read equals 0xFFFF. Example: bytes `FF 1C 2A` give
255 + 0x2A1C = 11,035.

### 6.5 Validation

Reject: distance beyond the start (alpha command 2 uses u16 + 2, which must not exceed the block index);
payload ending early. The alpha-run and block-run counters never cause an error; a block run that would
pass the end of the texture just stops there.

---

## 7. DXV3 `YCG6` and `YG10`: the HQ layer

### 7.1 Payload layout

The payload is two **plane sections**, back to back:

| Variant | Section 1 | Section 2 |
|---|---|---|
| `YCG6` | one-channel section over the Y plane (BC4, 8-byte blocks, (CW/4) x (CH/4) blocks) | two-channel section over the chroma planes (16-byte blocks, (CW/8) x (CH/8) blocks; channel 0 = Co, channel 1 = Cg) |
| `YG10` | two-channel section over the Y and A planes ((CW/4) x (CH/4) blocks; channel 0 = Y, channel 1 = alpha) | two-channel section over the chroma planes, as for YCG6 |

Section 2 begins at the byte right after the end of section 1's opcode area (7.2). **Measured:** the end
of section 2's opcode area is exactly the end of the payload.

### 7.2 Plane section layout

Offsets are from the start of the section.

| Offset | Size | One-channel section | Two-channel section |
|---|---|---|---|
| 0 | 4 | u32 opcode-area offset | u32 opcode-area offset |
| 4 | 4 | u32 opcode count | u32 opcode count, channel 0 |
| 8 | 4 | (literal area starts here) | u32 opcode count, channel 1 |
| 8 or 12 | … | literal area | literal area |
| opcode-area offset | … | one opcode stream | channel 0's opcode stream, then channel 1's |

- The **literal area** runs from the end of the header (8 or 12) to the opcode-area offset. It is read
  front to back by one cursor shared by both channels.
- Each **opcode stream** is self-delimiting (7.3). The section ends where the last opcode stream ends.
- The opcode streams decode to **opcodes**: one byte value each, 0..17 (7.4).

A decoder MUST reject: opcode-area offset smaller than the header length (8 / 12); opcode-area offset
beyond the end of the payload; an opcode count larger than the number of blocks of that channel (see 7.5
for why this bound is safe only with the 16-row coded height); an opcode stream that runs past the end
of the payload.

### 7.3 Opcode streams

The low two bits of an opcode stream's first byte select its coding:

| Low 2 bits | Coding | Length in bytes |
|---|---|---|
| 0 | **stored**: the opcodes are the next `count` bytes (the first byte is skipped; its upper 6 bits are ignored) | 1 + count |
| 1 | **fill**: every opcode equals the next byte | 2 |
| 2 | **entropy-coded** (FSE / tANS, table log 10): a frequency table that begins at the first byte, then a bitstream | table length + bitstream area length |
| 3 | the same as 2 (7.3.4); never written | as for 2 |

`count` is the opcode count from the section header.

**Measured** (W4: every opcode stream of the 142 Resolume HQ files on the reference machine, 45,980
packets, 139,970 streams): selector 2 in 138,651 streams; stored in 1,319, each a stream of 1 to 3
opcodes (1,309 of them a 3-opcode alpha channel in `YG10`, i.e. an almost uniform alpha plane); fill in
none; selector 3 in none. Resolume therefore writes the entropy coding for normal content and the
stored coding for tiny streams, never fill. GlEnc writes stored and fill (5,534 streams), never 2 or 3.

Resolume's Alley ships a licence notice for "FSE: Finite State Entropy coder" (Yann Collet, BSD 2-Clause);
the entropy coding below is that family (a tabled asymmetric numeral system). Public descriptions: Yann
Collet's FSE documentation and RFC 8878 section 4.1 (zstd's FSE tables), which use the same table
construction ideas (spread step, per-state baseline and bit count).

#### 7.3.1 Frequency table

The table is a little-endian bit string read from its least significant bit:
- the first u32 supplies 32 bits; its bits 0..1 are the coding selector (2 or 3) and are skipped;
- further bits come from u16 values that follow, each appended above the bits not yet used.

It holds the **counts** of symbols 0, 1, 2, … in order. Counts sum to exactly 1024. Field width rule:
- the first count is read with width 10;
- after each count, if the remaining total (1024 minus all counts so far) is less than 2^(width-1), the
  width for the next count decreases by 1 (at most once per count);
- reading stops when the remaining total reaches 0.

Example: counts 164, 16, 322, 139 are read with width 10; after 139 the remainder is 383 < 512, so the
next count uses width 9.

**Table length** in bytes = 4 + 2 x ceil(max(0, T - 32) / 16), where T = 2 + the sum of the widths of all
counts read. (A u16 none of whose bits were used is not part of the table.)

Reject: a count larger than the remaining total; more than 256 counts; running out of payload.

Symbols after the last non-zero count have count 0. A symbol value is an opcode (7.4).

A table with a single symbol cannot be written: the first count is read with width 10 (at most 1023) and
the width never grows, so no count can reach 1024. Every entropy-coded stream therefore has at least two
symbols with non-zero counts; a stream whose opcodes are all equal is written with the stored or fill
coding.

#### 7.3.2 Decoding table

1024 states, built from the counts:
- **Spread.** Walk positions p0 = 0, p(t+1) = (p(t) + 641) mod 1024 for t = 0..1023. Hand out symbols in
  increasing order: symbol 0 gets the first count(0) positions of the walk, symbol 1 the next count(1),
  and so on. Each state (position) now has a symbol.
- **Per state**, in increasing state order 0..1023: let s be the state's symbol; x = the next value of a
  per-symbol counter that starts at count(s) and goes up by 1 each time symbol s is met. The state's
  **bit count** = 10 - floor(log2(x)); its **baseline** = x x 2^(bit count) - 1024.

#### 7.3.3 Bitstream

Right after the table: a u32 **bit length** L. The **bitstream area** is ceil(L / 8) bytes starting at the
first byte of this u32 (so the length field is inside the area and is counted in L). Number the area's
bits 0 .. 8 x area-bytes - 1, bit k being bit (k mod 8) of byte floor(k / 8). Bits are consumed from bit
L - 1 downwards; a multi-bit value is read most significant bit first (the first bit consumed is its top
bit).

Decoding `count` opcodes:
1. state = the first 10 bits consumed;
2. repeat `count` times: emit the state's symbol; then state = the state's baseline + the next (bit count)
   bits. (This read also happens after the last symbol.)

The opcode stream ends at the end of the bitstream area. Reject when: ceil(L / 8) <= 4; the area runs
past the payload end; decoding would consume a bit below bit 0.

**Measured:** in every Resolume entropy stream decoded (28 streams, Alley and AME), exactly 32 bits are
left unconsumed: L = 32 + the number of bits the decoder consumes (10 + all bit counts, including the read
after the last symbol).

#### 7.3.4 Selector 3

Selector 3 means exactly what selector 2 means: the same frequency table (its first two bits, the
selector, are skipped in the same way) and the same bitstream. A decoder MUST decode it like 2. An encoder
MUST write 2.

Evidence (W4): no encoder found writes 3 (0 of 139,970 Resolume streams, 0 GlEnc or FFmpeg-written
streams). Black box: FFmpeg 8.1.1's decoder gives bit-identical frames (framemd5) for the Alley `YCG6`
and `YG10` vectors (6 frames) with the selector byte of each of their 21 entropy-coded streams changed
from 2 to 3; as a control, changing them to 0 makes it reject every frame. The vector
`synthetic-dxv3_ycg6-fse-selector3_testsrc2_1920x1080` holds the `YCG6` case; its expected block streams
are the Alley vector's.

### 7.4 Block reconstruction

#### 7.4.1 Terms

- A BC4 block (8 bytes) is seen as: **E** = bytes 0..1 (the two endpoints, as one u16), **I1** = bytes
  2..4, **I2** = bytes 5..7 (each a 24-bit group of eight 3-bit indices: two rows of the block).
- In a two-channel section the 16-byte block holds channel 0 in bytes 0..7 and channel 1 in bytes 8..15;
  each channel is reconstructed as its own sequence of 8-byte BC4 blocks. "The previous block" and
  "d blocks back" always mean the same channel's blocks (one-channel: 8 bytes per step back; two-channel:
  16 bytes per step back).
- Each channel has two **dictionaries** of 256 slots, empty at the start of the section:
  - the **endpoint dictionary** holds E values (2 bytes);
  - the **index dictionary** holds 3-byte index groups.
  The slot of a value v is h(v) = ((v x 2654435761) mod 2^32) >> 24 (2654435761 = 0x9E3779B1), where v is
  E read as a little-endian u16, or the 3 bytes of an index group read as a little-endian 24-bit number.
  **Insert E** = store this block's E in slot h(E); **insert I** = store this block's I1 (bytes 2..4 only,
  never I2) in slot h(I1). A later insert into the same slot replaces the value.
- Each channel has an opcode cursor (into its decoded opcodes) and a **repeat counter** (initially 0).
- Literal reads (`u8`, `u16`, `u32`, raw bytes) all come from the section's literal area, in the order
  listed, through one shared cursor.

#### 7.4.2 Order

1. The first block of each channel is literal: one-channel = the first 8 literal bytes; two-channel = the
   first 16 literal bytes (channel 0's block, then channel 1's). For each channel: insert E, insert I.
2. Then for block position 1, 2, … to the last: one-channel: reconstruct that block; two-channel:
   reconstruct channel 0's block, then channel 1's block.

#### 7.4.3 One block of one channel

If the channel's repeat counter is above 0: decrement it; the block = a copy of the previous block; no
opcode is used; nothing is inserted.

Otherwise take the channel's next opcode (reject if its opcodes are used up) and build the block from the
table. Notation: P = previous block; R = the block (u16 + 1) blocks back; ES = endpoint-dictionary slot
given by a u8; IS = index-dictionary slot given by a u8; L1 / L2 / L4 = 1 / 2 / 4 literal bytes; E-lit =
2 literal bytes used as E. Reads happen in the order of the "literal reads" column.

| Opcode | Literal reads, in order | E (bytes 0-1) | bytes 2-4 | bytes 5-7 | Inserts |
|---|---|---|---|---|---|
| 0 | u8 count (+ extension, 6.4) | whole block = P | | | none; repeat counter = count + 3 |
| 1 | none | whole block = P | | | none |
| 2 | u16 (for R) | whole block = R | | | E, I |
| 3 | 8 bytes | whole block = the 8 bytes | | | E, I |
| 4 | IS, E-lit, L2, L1 | E-lit | IS group | L2 then L1 | E |
| 5 | IS, E-lit, L2, L1 | E-lit | L2 then L1 | IS group | E, I |
| 6 | IS a, IS b, E-lit | E-lit | group a | group b | E |
| 7 | u16 (for R), E-lit | E-lit | R bytes 2-4 | R bytes 5-7 | E, I |
| 8 | ES, L2, L4 | ES value | L2 then first byte of L4 | rest of L4 | I |
| 9 | ES, IS, L2, L1 | ES value | IS group | L2 then L1 | I |
| 10 | ES, IS, L2, L1 | ES value | L2 then L1 | IS group | I |
| 11 | ES, IS a, IS b | ES value | group a | group b | none |
| 12 | ES, u16 (for R) | ES value | R bytes 2-4 | R bytes 5-7 | I |
| 13 | L2, L4 | P's E | L2 then first byte of L4 | rest of L4 | I |
| 14 | IS, L2, L1 | P's E | IS group | L2 then L1 | I |
| 15 | IS, L2, L1 | P's E | L2 then L1 | IS group | I |
| 16 | IS a, IS b | P's E | group a | group b | none |
| 17 | u16 (for R) | P's E | R bytes 2-4 | R bytes 5-7 | I |

So opcode 0 produces count + 4 identical-to-previous blocks in total (this one plus count + 3 repeats).
Opcodes 8 and 13 write bytes 2..7 as one 2-byte and one 4-byte literal (L2 = bytes 2-3, L4 = bytes 4-7).

Reject: opcode 18 or higher; a dictionary slot that is empty; R before the channel's first block (u16 + 1
greater than the current block position); the literal cursor passing the end of the payload; an opcode
needed after the channel's opcodes are used up.

**Measured:** Resolume uses all 18 opcodes (Alley 1080p Y plane: every value 0..17 occurs). No value of
18 or more was seen in any file. GlEnc uses opcodes 0, 1 and 3 only.

### 7.5 End of a section and what is ignored

A channel is complete when its last block is written. Then:
- opcodes left unused (the header's count is larger than the number of opcodes the channel needed) are
  ignored: a decoder MUST NOT apply them to the texture and MUST NOT reject the packet because of them. It
  MAY still decode them from the opcode stream to check that the stream itself is well formed;
- literal bytes left unread before the opcode-area offset are ignored.

The header bound of 7.2 (opcode count no larger than the channel's block count) still holds with one extra
opcode: the first block of a channel takes no opcode, so a channel needs at most (blocks - 1) opcodes.

**Measured (W4): Resolume's `YCG6` streams carry one extra opcode in the Y channel.**

| Resolume files on the reference machine | Packets | Packets with an extra opcode |
|---|---|---|
| `YCG6`, 134 files (`~/Movies/Mac Studio Builds/videos_renamed`, `~/Movies`, `~/Movies/Testfiles` and its subfolders, the GlEnc reference corpus), Alley and the Adobe Media Encoder codec | 43,950 | 43,707 |
| `YG10`, 8 files (`~/Movies/Testfiles` and subfolders, the GlEnc reference corpus) | 2,030 | 0 |

- It is always exactly one opcode, always in section 1 (the Y channel), never in a chroma channel. Its
  literal bytes are exactly the unread end of section 1's literal area; nothing else is left over, and
  every entropy-coded stream still leaves exactly 32 bits (7.3.3).
- It is the coding of one block **past the end of the Y plane**, and that block is the first block of
  the Co channel (section 2, channel 0, block 0). Checked by decoding the extra opcode on 4 frames of the
  Alley / AME vectors (opcode 3: its 8 literal bytes equal Co block 0) and 4 frames of a 908 x 2276 clip
  (opcodes 2 and 7: back-references to Y blocks that happen to equal Co block 0). So the opcode value
  varies; any of the 18 can occur. The Y opcode before it was, in every frame checked, a run (opcode 0)
  that ended exactly on the plane's last block.
- Exceptions (243 packets, all accounted for): the only `YCG6` file whose display size is a multiple of
  16 in both directions (512 x 1280, 241 packets) has no extra opcode at all; and one packet in each of
  two 1080-packet 908 x 2276 clips has none (in the one checked, the clip's first packet).
- Earlier wording corrected: the W1 results handoff (finding 2) said that with 16 x 16 padding "every
  Resolume stream uses every opcode and every literal exactly". That is true of `DXT1`, `DXT5`, `YG10` and
  of every chroma section, but not of the Y channel of Resolume `YCG6`.

What is legal:

| Situation | Encoder | Decoder |
|---|---|---|
| unused opcodes after a channel is complete | SHOULD NOT write them (Resolume does, see above); the count MUST NOT exceed the channel's block count | MUST ignore them and MUST NOT write them into the texture (the extra Resolume opcode would write past the Y plane); MAY decode them to validate the stream |
| literal bytes left before the opcode-area offset | SHOULD NOT leave any | MUST ignore them |
| literal read past the opcode-area offset | MUST NOT need one | SHOULD reject (every observed stream keeps its literals inside the literal area); MUST reject past the payload |
| bits left in an entropy-coded stream | SHOULD leave exactly 32 (7.3.3) | MUST NOT reject for unread bits; MUST reject a read below bit 0 |

Note on texture size: if a decoder wrongly uses CH = H rounded to 4 (not 16), it stops early, leaves more
opcodes unused, and with large frames its "count <= number of blocks" check can fail by one, because of
the extra Resolume opcode (one existing decoder works around this with a +1 tolerance). With the 16-row
coded height of 2.3 every observed stream passes the bound and no tolerance is needed.

### 7.6 HQ output

After reconstruction:
- Y: (CW/4) x (CH/4) BC4 blocks (YCG6: the one-channel stream; YG10: channel 0 of section 1);
- A (YG10 only): channel 1 of section 1, same geometry;
- Co and Cg: channels 0 and 1 of section 2, (CW/8) x (CH/8) BC4 blocks each, i.e. planes of CW/2 x CH/2
  samples.

The reference outputs store these exactly as decoded: `*.y.bc4` (YCG6), `*.ya.bc5` (YG10: 16-byte blocks,
Y then A), `*.chroma.bc5` (16-byte blocks, Co then Cg). A GPU decoder can upload Y as BC4 and the
two-channel streams as BC5 (RG) textures directly.

---

## 8. Block formats and conversion to pixels

The block streams are the normative output of a DXV decoder. Turning them into pixels follows the
standard BC1 / BC3 / BC4 definitions (public: the Khronos Data Format Specification, "S3TC" and "RGTC"
sections; Microsoft's block compression documentation). The reference RGBA outputs in `test_vectors/`
use the exact integer conventions below; a GPU or another CPU decoder may differ by 1 in some values,
which is why the vectors check the block streams first.

### 8.1 BC1 (DXT1)

8 bytes: colour 0 (u16 RGB565), colour 1 (u16 RGB565), 32 bits of 2-bit indices (pixel (x, y) at bits
2 x (4y + x)). Reference expansion of 5 / 6 bits to 8: bit replication ((r << 3) | (r >> 2), (g << 2) |
(g >> 4)). If colour 0 > colour 1 (as unsigned u16): palette = c0, c1, (2 c0 + c1) / 3, (c0 + 2 c1) / 3;
otherwise c0, c1, (c0 + c1) / 2, black (integer division, per channel). DXV `DXT1` and legacy DXT1 are
**opaque**: alpha = 255 for every pixel, and index 3 in the second mode is opaque black. **Measured:**
Resolume streams use the three-colour mode (colour 0 <= colour 1) but never index 3 in it (frames
checked from Alley, AME and two real clips: 38,115 three-colour blocks, none using index 3). A decoder that uploads to a GPU SHOULD use an opaque BC1
format.

Other readers' conventions (W5; all within the +-1 tolerance above, none changes the block streams):

- **FFmpeg 8.1.1** (its source read in the dirty room, then checked against its output): the palette
  rule and the integer interpolation are the reference ones above, but the 5 / 6-bit expansion is not
  bit replication. It is a rounded division by 31 (by 63) computed in integers: for a 5-bit value v,
  t = 255 v + 16 and the result is (t + t / 32) / 32; for a 6-bit value, t = 255 v + 32 and the result
  is (t + t / 64) / 64 (all divisions truncating). It differs from bit replication by exactly 1 at these
  values only:

  | Bits | Values v where FFmpeg gives bit replication - 1 | Values v where FFmpeg gives bit replication + 1 |
  |---|---|---|
  | 5 | 20, 24, 25, 28, 29 | 3, 7 |
  | 6 | 48 .. 53 | 11 .. 15 |

  With this expansion FFmpeg's RGBA output is reproduced exactly (0 bytes differ) on frame 0 of the Alley
  `DXT1` and `DXT5` vectors and of DXVKit's 1080p `DXT1` / `DXT5` encodes (8,294,400 bytes each); the
  reference convention differs from it in 0.589 % of bytes (DXVKit `DXT1`) to 8.1 % (Alley `DXT1`),
  never by more than 1 before alpha processing (`DXT5`: up to 2 after FFmpeg's un-premultiplication,
  8.5).
- **Resolume Arena 7.28** (GPU upload; composition output captured pixel-exact over Syphon, W5): bit
  replication with the interpolated colours computed without truncation and rounded to nearest at
  output. It matches that model in all but 0.19 % of bytes (at most 2 off, the GPU's own BC1 arithmetic),
  and the reference convention in 94.5 % (at most 2 off).

### 8.2 BC3 (DXT5)

Bytes 0..7: alpha 0, alpha 1, 48 bits of 3-bit indices (pixel (x, y) at bits 3 x (4y + x) of bytes 2..7
read as a little-endian 48-bit number). If alpha 0 > alpha 1: palette = a0, a1, then
((7 - i) a0 + i a1) / 7 for i = 1..6; else a0, a1, ((5 - i) a0 + i a1) / 5 for i = 1..4, then 0, 255.
Bytes 8..15: a BC1 colour block that always uses the four-colour palette.

**Colour endpoint order.** A BC3 colour half whose colour 0 <= colour 1 is read with the four-colour
palette by this spec, the BC3 definition, GPUs and FFmpeg 8.1.1. Some CPU decoders written for BC1 apply
the BC1 three-colour rule to it instead; the two readings differ for colour 0 < colour 1 in indices 2
and 3, and for colour 0 = colour 1 in index 3 only. (Correction, W5: the W4 text said FFmpeg applies the
three-colour rule, from the W3 black-box observation in `docs/cleanroom/QUESTIONS.md` item 10. FFmpeg's
source always uses the four-colour palette for its DXV `DXT5` path, and a black-box check confirms it: a
256 x 256 stored-flag `DXT5` packet with opaque alpha and 744 of its 4,096 colour halves swapped to
colour 0 < colour 1 decodes in FFmpeg exactly like the four-colour palette (0 bytes differ) and unlike
the three-colour rule (6,173 bytes differ). The W3 differences were most likely its un-premultiplication
(8.5) magnifying small colour differences at low alpha.)

- A writer MUST make every BC3 colour half read the same either way: colour 0 > colour 1 (as unsigned
  u16), or colour 0 = colour 1 with no pixel using index 3.
- A reader SHOULD use the four-colour palette. It will then agree with every Resolume file and with GPU
  upload; a reader that applies the BC1 rule also decodes Resolume's files correctly (measured below),
  but not every other writer's.

**Measured (W4):** 298 Resolume-written `DXT5` files on the reference machine (the `Resolume` string in the
container, which Alley and the Adobe Media Encoder codec both write; folders `~/Movies` and its
subfolders, `~/Downloads`, the story-arc project folders, the Resolume Wire documentation, the GlEnc
reference corpus), two frames each (the first and the
middle one): 78,358,368 colour halves; colour 0 > colour 1 in 69,565,985, colour 0 = colour 1 in 8,792,383
(none of them using index 3), colour 0 < colour 1 in **none**. Other writers: one GlEnc experiment file
(`~/Movies/GlEnc-scratch`, a cluster-fit test) has colour 0 < colour 1 in 1,036,411 of its 1,036,800
colour halves, every one of them using index 2 or 3, so FFmpeg and this spec decode it differently; the
other 41 GlEnc- and FFmpeg-labelled `DXT5` files have none.

### 8.3 BC4 (Y, A, Co, Cg)

8 bytes, unsigned, the same layout and palette rule as the BC3 alpha half.

### 8.4 YCoCg to RGB (YCG6, YG10)

Per pixel (x, y) of the display area: Y = luma sample (x, y); Co, Cg = the chroma samples at (x / 2,
y / 2) (integer division: **nearest neighbour**, every chroma sample covers one aligned 2 x 2 pixel
quad). With co = Co - 127.5 and cg = Cg - 127.5 (the chroma midpoint is 127.5, i.e. 0.5 in normalised
texture units, not 128):

- t = Y - cg
- R = t + co
- G = Y + cg
- B = t - co

then out-of-gamut values are folded (below) and each channel is rounded to nearest and clamped to
0..255. Alpha = A (YG10) or 255 (YCG6).

This is the plain YCoCg inverse (no halving of the chroma terms; the lifting "YCoCg-R" inverse gives
visibly desaturated pictures).

**Arena 7.28 behaviour** (W5, the authority for Resolume playback). A 1920 x 1080 `YCG6` file with hard
chroma steps at odd and even columns and rows, 1-pixel colour grids and chroma checkerboards at 1- and
2-pixel pitch (DXVKit's encoder, our own pattern) was played alone in an Arena composition of the same
size, layer opacity 1, no effects, and the composition output was captured over Syphon (a `DXT1` encode
of the same picture, captured the same way, came back geometrically exact and within 2 of a GPU BC1
decode, 8.1). Comparison with candidate reconstructions computed from the decoded BC4 blocks
(1,555,200 colour bytes, a quarter of the frame on a regular lattice):

| Reconstruction | Bytes exact | Within 1 | Within 2 | Max | Mean |
|---|---|---|---|---|---|
| nearest chroma, midpoint 127.5, out-of-gamut folding (below) | 81.10 % | 97.24 % | 98.56 % | 11 | 0.25 |
| nearest chroma, midpoint 127.5, plain clamping | 80.69 % | 96.27 % | 97.42 % | 54 | 0.49 |
| nearest chroma, midpoint 128, integer BC4 palette (the reference RGBA in `test_vectors/`) | 32.63 % | 85.80 % | 96.07 % | 54 | 1.10 |
| bilinear chroma, sited at the 2 x 2 quad centres, midpoint 127.5 | 38.74 % | 42.51 % | 43.51 % | 124 | 14.67 |

Bilinear chroma with co-sited (top-left) or half-texel-shifted placement fits worse still (mean 13.6 to
15.3). In the vertical-stripe area (row 100, columns 40 to 71, checked one by one) the chroma Arena
uses at each in-gamut pixel equals the chroma sample of its own 2 x 2 quad, with no weight on the
neighbouring sample. With midpoint 128 the green channel matches in 14 %
of in-gamut bytes; with 127.5, in 93 %. A `YG10` file whose Y and chroma blocks are identical to the
`YCG6` file's (alpha 255) gives a byte-identical Arena output: both variants are reconstructed the same
way.

**Out-of-gamut folding.** Where R or B leaves 0..255, Arena does not clamp the channels independently:
the overflow of one of R and B is carried into the other. A model that reproduces Arena (max 11 off in
the table above, against 54 for independent clamping): if R < 0, add R to B and set R to 0; if B < 0, add
B to R and set B to 0; if R > 255, add R - 255 to B and set R to 255; if B > 255, add B - 255 to R and set
B to 255; G is unchanged. The exact form Arena uses was not determined further; the remaining
differences (at most 2 for 98.6 % of bytes) are GPU arithmetic.

Rules: a player that wants to look like Resolume SHOULD use nearest chroma and the 127.5 midpoint, and
SHOULD fold out-of-gamut values as above. The reference RGBA outputs in `test_vectors/` follow this
section (exact BC4 palettes, nearest chroma, midpoint 127.5, folding): they were regenerated in W6 for
DXVKit 1.0.0 (W5 finding F6) and match Arena's Syphon capture within 1 in 97.21 % of bytes, max 11.
The W1 references used midpoint 128, integer palettes and plain clamping. The block streams remain the
normative output; the RGBA is a convenience.

### 8.5 Alpha meaning

The bitstream does not say whether colour is straight or premultiplied by alpha: no field in the packet
header, the sample description or the QuickTime atoms carries it, and both kinds of file exist. The only
way to tell is to look at the pixels: in premultiplied colour no channel can exceed alpha (apart from
block-compression error), in straight colour it often does.

Metric (all tables below): sample pixels with 8 < alpha < 200 (every 7th pixel), share whose
max(R, G, B) exceeds alpha + 24. Near 0 = **premultiplied-looking** (dark straight content can also look
like this, so it is a reading, not proof); clearly above a few per cent = straight.

**Writers** (W4: all 360 `DXT5` / `YG10` files on the reference machine, frames first and middle; files with
fewer than 500 semi-transparent sample pixels (329 files) cannot be classified):

| Writer | Variants | Convention | Evidence |
|---|---|---|---|
| Resolume Alley 7.28 | `DXT5`, `YG10` | straight | 4 files (the `resolume-alley` vectors' sources and copies): 97.4 % |
| Resolume codec in Adobe Media Encoder | `DXT5`, `YG10` | premultiplied-looking | 4 files: 0.1 % |
| Resolume, origin not recorded (`~/Movies/Testfiles`: one `DXT5`, two `YG10`) | `DXT5`, `YG10` | premultiplied-looking | 3 files: 0.0 % |
| Resolume clips shipped in Resolume Wire's documentation | `DXT5` | straight | 7 files: 8.6 to 9.2 % (premultiplied colour could not reach this) |
| GlEnc | `DXT5`, `YG10` | either, by version and variant | v0.5.0-candidate: `DXT5` straight (98.4 %), `YG10` premultiplied-looking (0.0 %) from the same source; older test files: 7 `DXT5` + 1 `YG10` premultiplied-looking |
| FFmpeg's encoder | `DXT1` only | (no alpha) | |
| DXVKit's encoder | all | what the caller passes (default straight, like Alley: Amrit's decision, W4, because DXVKit's users are Glance, CueGlance, MaCrate and GlEnc rather than Arena); a flag records the caller's convention but nothing is written to the file | `docs/design-encoder.md` |

**Readers:**

| Reader | Treats alpha files as |
|---|---|
| FFmpeg 8.1.1 decoder (black box, QUESTIONS 11) | `DXT5`: premultiplied: it outputs un-premultiplied colour, floor(c x 255 / a), clamped. `YG10`: planes output untouched |
| Resolume Arena 7.28 | the user chooses per clip with the clip setting **"Alpha Type"**; the default is **Premultiplied** (Amrit, W4). Arena's web API does not expose it (checked W4). So by default Arena reads every alpha file as premultiplied, like FFmpeg, and an Alley (straight) file needs "Alpha Type" changed to straight to composite as intended |
| DXVKit decoder | untouched by default; optional straight-to-premultiplied conversion on the RGBA path (QUESTIONS 7) |

So: a writer SHOULD write straight colour (what Alley, Resolume's own encoder application, writes), and
whoever plays the file in Arena must then set "Alpha Type" accordingly. A reader MUST NOT treat the
convention as known: it SHOULD let the user choose per file or clip, as Arena does; a reader that wants
to match Arena's default reads premultiplied. The reference RGBA outputs in `test_vectors/` are the decoded values with
no alpha processing.

---

## 9. Decoder checklist (errors and limits)

A conforming decoder MUST reject the packet (and SHOULD keep showing the previous frame) on any of:

| Where | Condition |
|---|---|
| any | packet shorter than its header (12 bytes DXV3, 4 bytes legacy) |
| container | sample-entry FourCC other than `DXD3`, `DXDI`, `DXD4` (2.2; the whole file) |
| DXV3 header | unknown texture tag; payload length + 12 > packet size; version byte below 4; stored flag other than 0 or 1 (3.2) |
| legacy header | payload length + 4 > packet size (no type byte is rejected, 4.1) |
| legacy LZF | input runs out inside an operation; back-reference before the output start; output length differs from the coded texture's |
| stored payload | shorter than the coded texture needs |
| DXT1 / DXT5 | payload ends before the texture is complete; a copy distance larger than the current block index |
| HQ section | opcode-area offset < header length or past the payload; opcode count > blocks in the channel; an opcode stream past the payload |
| HQ opcode stream | frequency table: count > remainder, more than 256 counts, truncated; bitstream area <= 4 bytes or past the payload; bits consumed below bit 0 |
| HQ blocks | opcodes used up; opcode >= 18; empty dictionary slot; back-reference before the channel's first block; literal read past the payload (SHOULD: past the opcode-area offset) |

Everything a decoder reads MUST be bounds-checked against the packet; a decoder MUST NOT read texture
memory outside the coded texture. Each packet is independent: no state survives from one packet to the
next (dictionaries, counters and flag words all start fresh).

Recommended output buffers: BC1 CW x CH / 2 bytes; BC3 CW x CH bytes; HQ Y CW x CH / 2 bytes (YCG6) or
CW x CH bytes (YG10, with alpha); HQ chroma CW x CH / 4 bytes.

---

## 10. Former open questions (all closed)

Nothing in this specification is open. Each item names its authority and evidence.

1. Closed (W5, Arena 7.28 behaviour, 4.1): legacy (`DXDI`) DXT5 (0x40), stored legacy payloads (0x80),
   every version nibble tried (0, 1, 2, 3, 4) and the type bytes without a texture bit (0x02, 0x03) all
   play in Arena; bit 0x40 selects DXT5, everything else DXT1, and no type byte is rejected. Real files
   of these forms still do not exist; the rule is Arena's.
2. Closed (W4 + W5, Arena 7.28 behaviour, 3.3): stored flag 1 plays for all four DXV3 variants; the
   `YCG6` / `YG10` stored layout is the plane streams back to back (Y or Y+A, then chroma). Stored flag
   values other than 0 and 1 play black in Arena: invalid.
3. Closed (W5, Arena 7.28 behaviour, 3.2): version byte 5 and 6 play, 3 and 0 play black (so: below 4
   invalid, 4 and above accepted); minor 1 and reserved 1 play.
4. Closed (W5, Arena 7.28 behaviour, 2.2): `DXD3`, `DXDI` and `DXD4` load; `DXV3`, `DXT1`, `YCG6`, `DXD1`,
   `DXDA` do not; the packet, not the FourCC, decides between legacy and DXV3.
5. Closed as far as the format goes (W4, 8.5): straight versus premultiplied alpha cannot be read from
   the file, because no field in the packet, the sample description or the QuickTime atoms carries it
   and files of both kinds exist (360 alpha files measured). Readers let the user choose (Arena: per-clip
   "Alpha Type", default Premultiplied).
6. Closed (W5, Arena's composition output captured pixel-exact, 8.4): Arena uses nearest-neighbour
   chroma, one sample per aligned 2 x 2 quad, chroma midpoint 127.5, and folds out-of-gamut R / B
   overflow into the other channel.
7. Closed (W5, Alley 7.28 exports of a known lossless source, ENCODING_NOTES.md 5): Resolume's forward
   transform is the exact YCoCg transform with chroma midpoint 127.5, unrounded before the BC4 encoder,
   with 2 x 2 box-averaged chroma.
8. Closed (W4): an entropy-coded opcode stream with a single symbol cannot be written (7.3.1); Resolume
   writes tiny streams with the stored coding and never uses fill (7.3).
9. Closed (W4): opcode-stream selector 3 decodes like 2 (7.3.4).
10. Closed (W4): the extra opcode in the Y channel of Resolume `YCG6` (7.5).
