# DXV files on the reference machine (Phase A, 2026-10-08)

Found with Spotlight (`kMDItemCodecs` matching DXV / DXD / Resolume) plus a walk of the Resolume
application folders; 706 candidates, 9 iCloud-only files skipped (reading would download them), every
remaining file parsed (sample entry, every packet header). **695 DXV files, 397,323 packets.** No clip was
copied into this repository; paths are listed by folder only.

"Writer" is a heuristic from metadata strings in the `moov` atom (`Resolume`, `GlEnc` / `glenc-cli`,
`Lavf` / `Lavc` = FFmpeg, `Adobe`). The Resolume codec inside Adobe Media Encoder usually also writes the
`Resolume` string.

| Folder | Files | Writer x variant |
|---|---|---|
| `/Applications/Resolume Wire/Documentation` | 9 | FFmpeg DXT5: 1, Resolume DXT1: 1, Resolume DXT5: 7 |
| `~/Desktop` | 1 | Resolume DXT1: 1 |
| `~/Documents/claude/native-builds` | 4 | FFmpeg DXT1: 3, GlEnc DXT1: 1 |
| `~/Documents/claude/native-builds/GlEnc/reference` | 16 | FFmpeg DXT1: 1, FFmpeg DXT5: 1, FFmpeg legacy DXT1: 1, GlEnc DXT1: 1, GlEnc DXT5: 2, GlEnc YCG6: 1, GlEnc YG10: 1, Resolume DXT1: 2, Resolume DXT5: 2, Resolume YCG6: 2, Resolume YG10: 2 |
| `~/Documents/claude/story-arc` | 1 | GlEnc DXT1: 1 |
| `~/Documents/claude/story-arc_RESTORED_sep01` | 50 | GlEnc DXT1: 1, Resolume DXT5: 49 |
| `~/Downloads` | 7 | GlEnc DXT1: 1, GlEnc DXT5: 1, Resolume DXT1: 1, Resolume DXT5: 4 |
| `~/Movies` | 15 | GlEnc DXT1: 4, GlEnc DXT5: 1, Resolume DXT5: 8, Resolume YCG6: 2 |
| `~/Movies/Captures` | 4 | Resolume DXT5: 4 |
| `~/Movies/GlEnc-scratch` | 8 | FFmpeg DXT1: 2, GlEnc DXT5: 5, GlEnc YG10: 1 |
| `~/Movies/KLV Set` | 23 | GlEnc DXT1: 1, Resolume DXT1: 7, Resolume YCG6: 15 |
| `~/Movies/Local` | 160 | Resolume DXT5: 160 |
| `~/Movies/Mac Studio Builds` | 139 | GlEnc DXT1: 1, Resolume DXT1: 9, Resolume DXT5: 4, Resolume YCG6: 125 |
| `~/Movies/Solar Fields` | 2 | GlEnc DXT1: 1, Resolume DXT1: 1 |
| `~/Movies/Testfiles` | 75 | Adobe plugin DXT1: 1, Adobe plugin legacy DXT1: 1, FFmpeg YG10: 2, GlEnc DXT1: 6, GlEnc DXT5: 31, GlEnc YCG6: 7, GlEnc YG10: 8, Resolume DXT1: 4, Resolume DXT5: 4, Resolume YCG6: 5, Resolume YG10: 6 |
| `~/StoryArc-M3-old-20260926` | 181 | GlEnc DXT1: 125, Resolume DXT5: 56 |

Totals by variant: {'DXT5': 340, 'DXT1': 176, 'legacy DXT1': 2, 'YCG6': 157, 'YG10': 20}
Totals by writer: {'Resolume': 481, 'FFmpeg': 11, 'GlEnc': 201, 'Adobe plugin': 2}

(Two further GlEnc fuzz files in `~/Downloads`, 65535 x 65535 and 1920 x 2, were excluded.)

## What the inventory established

- Sample-entry FourCCs: `DXD3` (693 files, all four DXV3 variants) and `DXDI` (2 files: one legacy clip,
  `~/Movies/Testfiles/Metric_01.mov`, 1920 x 1080, 201 frames, plus a 5-frame cut of it in the GlEnc
  reference corpus).
- DXV3 packet headers: all 397,117 have version byte 4, minor 0, stored flag 0, reserved 0, exact length.
- Legacy packets: all 206 have type byte 0x23 (DXT1, LZF, version nibble 3), exact length.
- No file mixes variants.
- Padding: width and height both rounded up to 16 (DXV_FORMAT.md 2.3).
- Resolume-written HQ streams use the entropy-coded opcode coding and all 18 opcodes.

## Not found anywhere

Legacy DXT5 / stored legacy packets, DXV3 stored-flag packets, HQ opcode values >= 18, version bytes
other than 4 (DXV3) or nibble 3 (legacy), sample-entry FourCCs other than `DXD3` / `DXDI`.

## What can produce new DXV files here

- Resolume Alley 7.28 (`/Applications/Resolume Alley`): GUI only, no command line. Writes all four DXV3
  variants.
- GlEnc / `glenc-cli` 1.0.0 (`~/Documents/claude/native-builds/glenc-public/.build/out/Products/Release/glenc-cli`):
  all four DXV3 variants; HQ with opcodes 0, 1, 3 and stored / fill opcode coding only.
- FFmpeg 8 (Homebrew): DXV3 DXT1 only.
- Nothing on this machine writes legacy `DXDI`.
