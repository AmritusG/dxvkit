project:    dxvkit
pass:       w3-alley-fair-compare
STATUS:     COMPLETE
parent:     ebf81555b663f293ac51f0d1a43e1885f470d9f3
result:     5af031d4dc58eae0d3317c13ab6a4c22dccecf15
session:    a3fd4145-83a7-4e1e-931f-72534cdaed15
written_at: 2026-10-08T15:03:52Z

# DXVKit W3 correction: fair encoder comparison with Alley (supersedes the "vs Alley / full encode" table of w3-encoder-results)

## What happened
Amrit confirmed **Arena layer 6 columns 1-10 all play correctly** (L6C8, 18x10, is hard to see but plays),
so Resolume also plays stored-flag packets (QUESTIONS 14 settled). He then exported our exact lossless source
through Alley 7.28. The four files are in `out/w3/`; the names do not match the contents:
"Normal Quality with Alpha" = DXT1, "DXV Normal Quality With Alpha" = DXT5, "High Quality No Alpha" = YCG6,
"HQ with alpha" = YCG6 again (byte-identical). **No YG10 export.** All four decode with our decoder (0
errors; the YCG6 files carry Resolume's usual dangling-opcode tail, spec 7.5).

## Fair result (same RGBA into both encoders, frames 0-2, PSNR RGB, our decoder)
| variant | encoder | mean packet B | PSNR | vs Alley |
|---|---|---|---|---|
| DXT1 | Alley | 98,973 | 39.71 dB | |
| DXT1 | DXVKit fast / best | 99,080 / 100,027 | 39.70 / 39.88 dB | +0.1 % / +1.1 % size, -0.01 / +0.17 dB |
| DXT5 | Alley | 192,394 | 39.71 dB, alpha lossless | |
| DXT5 | DXVKit fast / best | 190,352 / 177,876 | 39.70 / 39.86 dB, alpha lossless | -1.1 % / -7.5 % size |
| YCG6 | Alley | 132,032 | 37.71 dB | |
| YCG6 | DXVKit fast / best | 123,400 / 123,218 | 37.95 / 38.03 dB | -6.5 % / -6.7 % size, +0.24 / +0.32 dB |
Repack of these Alley files with our packer (best): DXT1 +0.01 %, DXT5 -8.28 %, YCG6 -0.41 %, blocks identical.
The earlier "+4..+6 % larger" came from the reconstructed source (QUESTIONS 20), not from the encoder.
QUESTIONS 21 records this. No code changed. Commit 5af031d4dc58eae0d3317c13ab6a4c22dccecf15 (QUESTIONS 21).

## Needs Amrit
- Optional: re-export YG10 ("DXV High Quality With Alpha") from the same qtrle file into `out/w3/` to complete
  the table.
- Arena comp "LoadEhr" still holds layer 6 from W3, unsaved; keep or discard.
- Amrit's answers from this session: "1-10 plays correctly. (Bit hard to see L6C8, but it plays)"; the Alley
  exports are in the test-files folder (out/w3).

## RECEIPTS
```
files 4 (bad 0) | packets 12 | errors 0 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 6
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_DXV Normal Quality With Alpha.mov: DXT5
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_HQ with alpha.mov: YCG6
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality No Alpha.mov: YCG6
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_Normal Quality with Alpha.mov: DXT1
HQ with alpha == High Quality No Alpha: identical
-rw-r--r--@ 1 <user>  staff  577904 Oct  8 16:57 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_DXV Normal Quality With Alpha.mov
-rw-r--r--@ 1 <user>  staff  396819 Oct  8 16:57 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_HQ with alpha.mov
-rw-r--r--@ 1 <user>  staff  396819 Oct  8 16:59 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality No Alpha.mov
-rw-r--r--@ 1 <user>  staff  297641 Oct  8 16:56 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_Normal Quality with Alpha.mov
```
```
5af031d QUESTIONS 21: fair Alley comparison on Amrit's export of our exact source
## main...origin/main
```
END OF HANDOFF w3-alley-fair-compare sha256:d880b80f 3237
