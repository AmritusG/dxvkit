```
DXVKIT W6 — CLEAN ROOM: MAKE DXVKIT BEHAVE EXACTLY LIKE ARENA (FINDINGS F1-F7), NEW REFERENCES, THEN TAG v1.0.0 FOR THE APPS — RUN TO FINISHED

MODE: build wave on DXVKit (~/Documents/claude/DXVKit). CLEAN ROOM: a fresh session (/clear first); the DXVKit HARD RULES apply (no FFmpeg / Glance / GlEnc / CueGlance / MaCrate source; read only this repo + public BCn / FSE / QuickTime docs; black-box use of Arena and the ffmpeg binary allowed and logged). Commit + push + tag at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"Does this mean you can give DXVKit a handoff to Glance and the other apps?"
Cowork's answer: almost. W5 measured Arena itself and found seven places where DXVKit differs from Arena (F1-F7 in docs/cleanroom/QUESTIONS.md). Fix those, regenerate the references, tag a release, and THAT tag is what Glance and the other apps pin.

DECIDED (do not re-decide; Arena 7.28 is the authority for a Resolume-compatible library)
1. F1 legacy type byte 0x03 (no texture flag): ACCEPT (Arena plays it; rule per spec 4.1: 0x40 -> DXT5, else DXT1; never reject for the type byte). Replace the vector bad_dxdi-no-texture-flag: it becomes a valid decode vector.
2. F2 DXV3 version byte < 4: REJECT (Arena shows black). Version >= 4, minor 1, reserved 1: accept (Arena plays).
3. F3 DXV3 stored flag 2: REJECT (Arena shows black). Only 0 / 1.
4. F4 YCG6 / YG10 with stored flag 1: ACCEPT, laid out as the plane streams back to back (spec 3.x as written by W5).
5. F5 FourCC DXD4 and the cross cases (DXDI track with DXV3 packets, DXD3 track with legacy packets): ACCEPT, the packet decides. FourCCs Arena does not register (DXV3, DXT1, YCG6, DXD1, DXDA): reject.
6. F6 HQ RGBA path: chroma midpoint 127.5 + the R / B overflow folding exactly as spec 8.4 now states, nearest chroma. YES: regenerate the HQ RGBA references to this convention (the block-stream references do not change). Keep the old reference files out of the repo (git history has them).
7. F7 encoder: write chroma with midpoint 127.5 (spec / ENCODING_NOTES 5: exact unrounded YCoCg, 2 x 2 box chroma) so Arena's grey 128 reads 128, not 127 / 128 / 129.

READ FIRST (all of it, extract into docs/design.md before code)
docs/spec/DXV_FORMAT.md (2.2, 3.1-3.3, 4.1, 8.1, 8.2, 8.4, 9, 10), ENCODING_NOTES.md 5, QUESTIONS.md F1-F7 and the W5 results handoff (forms table, the Syphon numbers: nearest + 127.5 + folding = 97.24 % of bytes within 1, max 11), docs/design.md, docs/design-encoder.md, the decoder + encoder sources, tests and fuzz harnesses.

IMPLEMENT
1. Decoder acceptance rules F1-F5 exactly as decided; status codes unchanged in meaning, new ones only if a new rejection needs one.
2. RGBA path F6 (C core, C++, Swift). Block outputs unchanged.
3. Encoder F7 (both effort levels; blocks-in path untouched).
4. Vectors: add the W5 forms as committed vectors where they are small and ours (the 256 x 256 synthetic files W5 listed under ~/Movies/Testfiles/dxvkit-w5-*: copy them in, cp -p, if < 1 MB each; else generate equivalents), each with the Arena verdict as its expected result (decode / reject). Regenerate HQ RGBA references (F6). MANIFEST + vectors.tsv updated; the README states the reference convention.
5. API version: DXVKIT_VERSION 1.0.0 in the header + Swift; a CHANGELOG.md (W2 decoder, W3 encoder, W6 Arena rules).

BUILD + TEST
1. dxvtool verify: every vector incl. the new forms passes with its Arena verdict; block streams unchanged on every old vector (byte-identical to before this wave).
2. Arena agreement table: for every form file, DXVKit's verdict == Arena's (play / black / not a clip), 0 disagreements.
3. Colour: against the W5 Syphon captures if they are in the repo or listed by path: DXVKit RGBA vs Arena's pixels, report % within 1 and max (expect the W5 model numbers or better); grey 128 round trip through the encoder + Arena (load a flat-grey YCG6 into an Arena clip untriggered and ask Amrit to fire it only if a capture is needed; never trigger).
4. Encoder: round-trip 112 cases still identical; fair Alley comparison re-run (W3 table) to show size / PSNR did not get worse with 127.5.
5. ASan + UBSan all tests; libFuzzer 30 min decode + 30 min encode, 0 crashes.
6. Real files: dxvtool check over every DXV file in ~/Movies: 0 errors except known corrupt fixtures (list them).
7. Commit + push + annotated tag v1.0.0. Handoff docs/handoffs/dxvkit_2026-10-08_w6-arena-rules-results.md, cp -p to ~/claude-handoffs/: what changed per finding, the agreement table, colour numbers, verify / fuzz / real-file receipts, the tag + commit, an ADOPTION NOTE for apps (pin tag v1.0.0; what changed vs cdbb4bc for a caller: acceptance rules, HQ RGBA midpoint, encoder midpoint; for GPU-path callers: the shader's chroma midpoint must be 127.5 and chroma sampled nearest to match Arena), the attestation, Amrit's answers.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Licences MIT / BSD / Apache / zlib / ISC / Boost only. No footage that is not ours in the repo. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
