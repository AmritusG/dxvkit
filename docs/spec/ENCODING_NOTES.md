# DXV encoding notes

Status: Phase A, W1 (2026-10-08). Companion to `DXV_FORMAT.md` for a later encoder phase. No code, no
pseudocode. "Must" here means "or Resolume / a conforming decoder may not play it"; "observed" means what
Resolume's own encoders (Alley, and the codec inside Adobe Media Encoder) write, measured on the files
described in `docs/cleanroom/INVENTORY.md`.

Revised in W4 (2026-10-08, Phase A): BC3 endpoint order, alpha convention, the extra Y opcode, selector 3
and the single-symbol rule (4, 7.1, 7.3).

## 1. Container

- One QuickTime video track; sample-entry FourCC `DXD3` for all four DXV3 variants. Use the field values
  of DXV_FORMAT.md 2.2 (vendor `FFMP`, quality 512, 72 dpi, depth 24, empty compressor name, entry size
  86). These are what Resolume writes; matching them avoids any host that sniffs fields.
- Width / height in the sample entry and track header = the display size, not the padded size.
- One sample per frame, every sample a sync sample. Constant frame rate (`stts` with one entry).
- Do not mix variants within a file.
- Write the legacy `DXDI` form only if a legacy target needs it; nothing produced since DXV3 uses it.

## 2. Packet header (DXV3)

- Bytes 0..3 = the tag reversed (`1TXD`, `5TXD`, `6GCY`, `01GY`).
- Byte 4 = 4, byte 5 = 0, byte 7 = 0 (observed in every packet).
- Byte 6 (stored flag) = 0 normally. A stored packet (flag 1, payload = the raw block stream) is legal for
  DXT1 / DXT5 per the format, but no Resolume encoder was seen writing one. Resolume Arena 7.28 plays
  them (Amrit, W4: DXVKit's 333 x 221 stored-flag DXT1 and DXT5 test files play correctly). Prefer the
  compressed form; stored is a valid fallback, e.g. when compression would be larger. Not for YCG6 / YG10.
- Bytes 8..11 = the exact payload length; the sample size = 12 + payload length (observed, always).

## 3. Texture size

- Pad the picture to CW = ceil16(W) by CH = ceil16(H) and compress the whole padded texture. Every block
  of the padded texture must be present in the stream.
- Fill the padding with a neutral, cheap-to-code value (flat black; for HQ, Y = 0 and Co = Cg = 128;
  alpha 0 or 255). Its content is never displayed but costs bits.

## 4. Block encoding (BC1 / BC3 / BC4)

- Any valid block encoder works; the format only stores the blocks. Quality is the encoder's business.
- BC1 for `DXT1`: the texture is opaque. Do not use index 3 of the three-colour mode (colour 0 <= colour
  1): Resolume never does, and a decoder that uploads with a punch-through BC1 format would show holes.
  Solid blocks with colour 0 = colour 1 are fine (observed often).
- BC3 for `DXT5`: the spec decodes the colour half with the four-colour palette whatever the endpoint
  order, but some readers (FFmpeg) apply the BC1 three-colour rule when colour 0 <= colour 1. Order every
  colour half as a four-colour BC1 block: colour 0 > colour 1, or colour 0 = colour 1 with no pixel on
  index 3. Resolume always does (DXV_FORMAT.md 8.2: 0 of 78 million colour halves have colour 0 <
  colour 1).
- BC4 for Y / A / Co / Cg: unsigned.
- Alpha convention: the file has no field for it. Alley writes straight (unpremultiplied) colour; the
  codec in Adobe Media Encoder wrote premultiplied-looking colour; GlEnc varies (writers and readers table:
  DXV_FORMAT.md 8.5). Write straight, like Alley. Arena reads every alpha clip as premultiplied unless the
  clip's "Alpha Type" is changed, so tell users of straight files to set it.

## 5. YCoCg (HQ variants)

- The decoder inverse is fixed (DXV_FORMAT.md 8.4): R = Y - cg + co, G = Y + cg, B = Y - cg - co with
  co = Co - 128, cg = Cg - 128. The matching forward transform is Y = R/4 + G/2 + B/4,
  Co = R/2 - B/2 + 128, Cg = -R/4 + G/2 - B/4 + 128. Rounding is the encoder's choice; keep Co and Cg
  inside 0..255 (Co = 128 + (R - B)/2 can reach 255.5 for pure red: round down or clamp).
- Chroma is subsampled 2 x 2 (one Co / Cg sample per 2 x 2 luma pixels, covering the padded size).
  Averaging the four pixels is a sensible choice; Resolume's exact filter is unknown.

## 6. The block-copy layers (DXT1, DXT5)

What the decoder accepts, restated as encoder obligations:

- Block 0 is written verbatim (8 bytes DXT1, 16 bytes DXT5).
- Flags are 2 bits, packed 16 per u32, least significant first. The u32 must be placed in the byte stream
  at the point where the decoder will need its first flag: before any literal or extra byte that belongs
  to a later flag. In practice: when a 17th flag is about to be written, reserve a new u32 at the current
  end of the output and fill its flags as they are decided.
- The bytes belonging to a flag (u8 / u16 distance, 4 literal bytes) follow in the order the decoder takes
  the flags.
- Distance coding: 1 = flag 1; 2..257 = flag 2 + u8 (distance - 2); 258..65,793 = flag 3 + u16
  (distance - 258). Larger distances cannot be coded: send the data instead.
- Whole-block copies (block flag with a distance) are the cheap case; per-half (DXT1) or per-quarter
  (DXT5 colour) flags are taken only after a block flag of 0.
- DXT5 alpha half: the cheapest commands are the block run (alpha command 0: n identical-to-previous
  whole blocks, n >= 1, coded as u8 n - 1, 255 meaning "add u16 values while they are 0xFFFF") and the
  alpha run (command 1: the alpha half of this block and the next r blocks repeats the previous block's
  alpha half; u8 r with 255 + u16 extension). Command 2 copies an alpha half from u16 + 2 blocks back;
  command 3 sends it literally.
- Count extension: after the first byte reaches its maximum, add u16 values; each u16 of 0xFFFF means
  "another u16 follows". A count that is an exact multiple of 0xFFFF past the first byte therefore needs a
  trailing u16 0.
- The payload may end as soon as the last block is complete; unused flags in the last u32 are zero.
  Observed Resolume payloads end exactly there.
- A hash table keyed on the 4-byte half (and on the full 8-byte block) is the usual way to find
  distances; distances are counted in blocks, so only matches at the same position within a block count.

## 7. The HQ layer (YCG6, YG10)

### 7.1 Sections

- YCG6: a one-channel section for Y, then a two-channel section for (Co, Cg).
- YG10: a two-channel section for (Y, A), then a two-channel section for (Co, Cg).
- Section header: opcode-area offset (from the section start), opcode count(s). The literal area must lie
  entirely between the header and the opcode-area offset, and should end exactly there (observed). The
  next section starts right after the last opcode stream of the previous one; the payload ends right after
  section 2's last opcode stream (observed).
- The opcode count in the header is the number of opcodes actually emitted for that channel. It must not
  exceed the channel's block count. Resolume's `YCG6` encoder emits one extra Y opcode in almost every
  frame, coding one block past the Y plane (the first Co block; DXV_FORMAT.md 7.5). Do not copy that:
  decoders ignore it, but it wastes bytes and a careless decoder would write past the plane.

### 7.2 Choosing opcodes

- Mirror the decoder exactly: keep, per channel, the two 256-slot dictionaries (endpoint pairs, 3-byte
  index groups) and update them with the same insert rules (DXV_FORMAT.md 7.4.3 table, "Inserts" column)
  after every block you emit, using the same hash h(v) = (v x 2654435761 mod 2^32) >> 24. An encoder whose
  dictionaries drift from the decoder's produces garbage silently.
- Remember only I1 (bytes 2..4) is ever inserted into the index dictionary, but a looked-up group can be
  placed in either I1 or I2.
- Runs: opcode 0 covers count + 4 identical-to-previous blocks (minimum 4); opcode 1 covers one.
- A sensible greedy order per block: run / previous-block copy; exact match by back-reference (opcode 2,
  u16 distance up to 65,536 blocks); then the combinations of dictionary hits for E, I1, I2 with literals
  for the misses (opcodes 4..17); finally opcode 3 (8 literal bytes). Pick the cheapest in literal bytes
  plus entropy cost.
- Resolume uses all 18 opcodes; GlEnc used only 0, 1 and 3 (valid but larger).

### 7.3 Opcode stream coding

- Stored (first byte 0x00, then the opcodes) and fill (0x01, value) are always valid. Use fill when every
  opcode is the same (the entropy coding cannot express a single-symbol stream, DXV_FORMAT.md 7.3.1).
  Resolume itself writes stored for streams of 1 to 3 opcodes and never fill.
- Entropy coding (selector 2 in the low bits of the first byte; 3 is read the same way but no encoder
  writes it, DXV_FORMAT.md 7.3.4; Resolume uses it for normal content;
  on the Alley 1080p YCG6 vector it codes 15,423 Y opcodes in 5,796 bytes, 38 % of the stored size):
  - Normalise the opcode histogram to counts summing to exactly 1024 (every used symbol at least 1).
  - Write the counts with the variable width rule of DXV_FORMAT.md 7.3.1, LSB first, starting right
    above the 2 selector bits of the first u32, continuing in u16 units; the table ends at the last u16
    that holds at least one count bit (table length formula in 7.3.1).
  - Build the same decoding table as the decoder (spread step 641, table log 10); encode the opcodes in
    reverse order with the matching tANS encoder (Yann Collet's FSE documents the procedure), so that the
    decoder, reading bits from the end of the area downwards, meets them in forward order.
  - The decoder reads its initial 10-bit state first, and it also reads one state transition after the
    last opcode: the stream must contain those bits.
  - Bit length L = 32 + the number of bits the decoder will consume (observed in every Resolume stream);
    the area is ceil(L / 8) bytes from the start of the u32 L. Bits are numbered from the area start;
    the decoder starts at bit L - 1.
  - Choose the cheaper of stored / fill / entropy per stream.

## 8. Self-check for an encoder

Decode every packet you write with a decoder built from DXV_FORMAT.md and compare the block streams with
the ones you meant to write (they must be identical: the layers are lossless). Then check: payload ends
exactly where expected, no unused opcodes, literal area exactly filled, sample size = 12 + payload length,
every BC3 colour half ordered as in section 4.
