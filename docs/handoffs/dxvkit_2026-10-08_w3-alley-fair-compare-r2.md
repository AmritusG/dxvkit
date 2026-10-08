project:    dxvkit
pass:       w3-alley-fair-compare-r2
STATUS:     COMPLETE
parent:     39637797907fa894cfeee2bc8679e8b54bd56b58
result:     4a28be02e8cfa857965353a069608d772e53fad0
session:    a3fd4145-83a7-4e1e-931f-72534cdaed15
written_at: 2026-10-08T15:09:08Z

# DXVKit W3: fair Alley comparison, all four variants (supersedes w3-alley-fair-compare)

Amrit re-exported our exact lossless source (`out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle.mov`)
through Alley 7.28 in all four variants, now named correctly (names match contents, checked with
`dxvtool info`). Both encoders were measured against the same RGBA (frames 0-2, PSNR RGB, our decoder).

| variant | Alley B / dB | DXVKit fast B / dB | DXVKit best B / dB | fast vs Alley | best vs Alley |
|---|---|---|---|---|---|
| DXT1 | 98,973 / 39.71 | 99,080 / 39.70 | 100,027 / 39.88 | +0.1 %, -0.01 dB | +1.1 %, +0.17 dB |
| DXT5 | 192,394 / 39.71 | 190,352 / 39.70 | 177,876 / 39.86 | -1.1 %, -0.01 dB | -7.5 %, +0.15 dB |
| YCG6 | 132,032 / 37.71 | 123,400 / 37.95 | 123,218 / 38.03 | -6.5 %, +0.24 dB | -6.7 %, +0.32 dB |
| YG10 | 215,546 / 37.71 | 207,406 / 37.95 | 207,224 / 38.03 | -3.8 %, +0.24 dB | -3.9 %, +0.32 dB |
Alpha (DXT5, YG10) lossless for both encoders. Packer only (Alley's own blocks re-packed, best): DXT1
+0.01 %, DXT5 -8.28 %, YCG6 -0.41 %, YG10 -0.30 %, blocks identical. Verdict: parity or better in every
variant. Only DXT1 best spends 1 % more bytes, for +0.17 dB.
No code changed; QUESTIONS 22 (commit 4a28be02e8cfa857965353a069608d772e53fad0).

Amrit's answers this session: "1-10 plays correctly. (Bit hard to see L6C8, but it plays)"; "I re-made the
4 clips to be sure I named them properly". Still open: Arena comp "LoadEhr" holds W3's layer 6, unsaved.

## RECEIPTS
```
files 4 (bad 0) | packets 12 | errors 0 | not exactly consumed 0 | exact apart from a dangling-opcode tail (spec 7.5) 3
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_DXV Normal Quality With Alpha.mov: DXT5
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality No Alpha_Custom.mov: YCG6
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality With Alpha.mov: YG10
dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_Normal Quality No Alpha.mov: DXT1
-rw-r--r--@ 1 amritrosell  staff  577904 Oct  8 17:05 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_DXV Normal Quality With Alpha.mov
-rw-r--r--@ 1 amritrosell  staff  396819 Oct  8 17:05 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality No Alpha_Custom.mov
-rw-r--r--@ 1 amritrosell  staff  647362 Oct  8 17:06 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_High Quality With Alpha.mov
-rw-r--r--@ 1 amritrosell  staff  297641 Oct  8 17:05 out/w3/dxvkit-w3_alley-source_testsrc2_1920x1080_qtrle_Normal Quality No Alpha.mov
4a28be0 QUESTIONS 22: complete fair Alley comparison (YG10 added); ## main...origin/main
```
END OF HANDOFF w3-alley-fair-compare-r2 sha256:c0813113 2618
