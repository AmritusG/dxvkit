# Changelog

DXVKit, a clean-room MIT library for Resolume's DXV video codec. Versions follow semantic versioning;
the git tag `vX.Y.Z` is what applications pin.

## 1.0.0 (2026-10-08, W6): Resolume Arena 7.28 rules

The first release for applications (Glance, GlEnc, CueGlance, MaCrate, LoadEhr). Arena 7.28 is the
authority for a Resolume-compatible library (findings F1-F7, `docs/cleanroom/QUESTIONS.md`); every W5
Arena form now gets the same verdict from DXVKit as from Arena (33 / 33, `dxvtool arena`).

Decoder: acceptance rules (block streams of every earlier vector unchanged, byte for byte)
- Legacy (`DXDI`) type byte: bit 0x40 selects DXT5, anything else DXT1; no type byte is rejected any
  more (0x03 used to be). (F1)
- DXV3 version byte (header byte 4) below 4 is rejected; 4 and above, any minor and reserved byte, are
  accepted. (F2)
- DXV3 stored flag (byte 6) other than 0 / 1 is rejected (it used to be read as "stored"). (F3)
- Stored-flag `YCG6` / `YG10` packets are decoded: plane 0 then plane 1, back to back (they used to be
  rejected with `DXVK_ERR_UNSUPPORTED`). (F4)
- Sample-entry FourCC `DXD4` is accepted besides `DXD3` / `DXDI`; the packet, not the FourCC, decides
  legacy versus DXV3 (a DXV3 tag in the first four bytes = DXV3). The `dxvk_generation` argument is now
  a hint; `DXVK_GEN_AUTO` added. (F5)
- New status `DXVK_ERR_BAD_HEADER` (appended; existing values keep their numbers).

RGBA path (CPU, C / C++ / Swift)
- `YCG6` / `YG10`: exact BC4 palettes, nearest chroma, chroma midpoint 127.5, R / B overflow folding,
  rounding at the end (spec 8.4). Against Arena's own output: 97.21 % of colour bytes within 1, max 11
  (was 85.35 %, max 54). `DXT1` / `DXT5` RGBA unchanged. (F6)

Encoder
- `YCG6` / `YG10`: exact unrounded YCoCg with chroma midpoint 127.5 and 2 x 2 box chroma, like Alley
  (ENCODING_NOTES 5); a grey 128 now reads 128 in Arena (was 128 / 129 / 127). Both effort levels.
  `DXT1` / `DXT5` output and the blocks-in path are byte-identical to 0.1.0. (F7)

Other
- Version: `DXVKIT_VERSION` "1.0.0", `dxvk_version_string()`, `dxvk::version()`, Swift `DXVKitVersion`.
- `dxvtool arena`, `dxvtool dump-packet`, `dxvtool version`.
- Test vectors: the 33 Arena forms (`streams/arena/`, `arena_forms.tsv`), `bad_dxdi-no-texture-flag`
  became the decode vector `synthetic-dxdi_type03-implied-dxt1_pat64`, HQ RGBA references regenerated
  to the 8.4 convention (334 rows).

## 0.1.0 (W3, 2026-10-08, commit 11baeca): encoder

DXV3 encoder for `DXT1`, `DXT5`, `YCG6`, `YG10` (RGBA in, or caller block planes in), two effort levels,
FSE / stored / fill opcode coding, QuickTime writer; C, C++ and Swift APIs; `dxvtool encode`.

## W2 (2026-10-08, commit cdbb4bc): decoder

C99 decoder for legacy `DXDI` (LZF, stored) and DXV3 `DXT1`, `DXT5`, `YCG6`, `YG10` (incl. FSE opcode
streams), QuickTime demuxer, CPU BCn to RGBA, header-only C++ wrapper, SwiftPM package, `dxvtool`.
