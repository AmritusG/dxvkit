```
DXVKIT W2 — CLEAN-ROOM DXV, PHASE B (THE CLEAN ROOM): WRITE THE DECODER FROM THE SPEC ALONE, BYTE-EXACT ON EVERY VECTOR — RUN TO FINISHED

MODE: build wave on DXVKit (~/Documents/claude/DXVKit, AmritusG/dxvkit). THIS MUST BE A FRESH SESSION: Amrit runs /clear before pasting this. If this session's context holds anything from Phase A (FFmpeg, Glance, GlEnc source), STOP and say so.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"Clean room. Can it be made to work / merge/ replace Glance and the others you built?"

THE CLEAN ROOM (binding; already HARD RULES in DXVKit's CLAUDE.md)
1. You MAY read: DXVKit's CLAUDE.md, docs/spec/DXV_FORMAT.md, docs/spec/ENCODING_NOTES.md, test_vectors/ (streams, reference outputs, vectors.tsv, MANIFEST), docs/cleanroom/, docs/handoffs/ in this repo; and PUBLIC documentation only: Apple QuickTime File Format (MOV atoms, stsd / stsz / stco / co64 / stsc), Microsoft / Khronos BCn docs (BC1 / BC3 / BC4 / BC5), RFC 8878 (Zstandard: its FSE section) and Yann Collet's FSE write-ups, the LZF format description.
2. You MUST NOT open, search, grep or fetch: FFmpeg (any libavcodec file, any mirror, doxygen pages), Glance / GlanceCore / GlancePlayback, glenc-public, GlEnc, CueGlance, MaCrate, Alley internals, or any other DXV implementation. No web searches for DXV source code. If a search result shows DXV code, close it and log that it happened.
3. No code copied from anywhere. Third-party code may be vendored only under MIT / BSD / Apache / zlib / ISC / Boost and only if it is NOT a DXV implementation (e.g. an FSE / LZF library is allowed if its licence fits; say which and why, or write your own from RFC 8878).
4. Where the spec is silent or ambiguous: do NOT guess from memory of other implementations. Write the question into docs/cleanroom/QUESTIONS.md, solve it ONLY from the test vectors (experiment until the vector matches), and record how. Cowork relays anything unresolved to the Phase A side.
5. Append your attestation to docs/cleanroom/PROVENANCE.md at the end: session id, what you read (list), "did not access" list, any accidental exposure.

READ FIRST (all of it, then extract what you need into docs/design.md before writing code)
1. CLAUDE.md, DXV_FORMAT.md and ENCODING_NOTES.md in full; vectors.tsv; the Phase A results handoff (docs/handoffs/dxvkit_2026-10-08_w1-spec-results.md) incl. its findings: 16x16 padding on BOTH axes for DXV3 and legacy; real Resolume HQ uses the FSE opcode streams and all 18 opcodes; FSE streams leave exactly 32 bits unread; BC1 three-colour blocks with index 3 never used (DXT1 opaque); reference RGBA = the documented BC1 expansion with DXT1 alpha 255.
2. The public docs listed above, for every part you touch (MOV sample tables incl. co64 and stsc chunk runs; BC1 / BC3 / BC4 bit layouts; FSE table build + decoding).

IMPLEMENT
1. C99 core, libc only, no global state, thread-safe per decoder object, MIT headers on every file:
   a. MOV demux: find the DXV track (DXD3 / DXDI), size, frame rate, sample count, sample offsets / sizes (stco and co64, stsc runs), read one packet by index into a caller buffer. Reject what is not DXV.
   b. Packet decode, every variant: legacy (DXDI, LZF and stored), DXV3 DXT1, DXT5, YCG6, YG10, the stored flag. Output A (the GPU path, the main one): the BCn block stream(s) exactly as the reference streams (layout per plane documented in the header). Output B (CPU): RGBA8 at the visible size, cropped from the padded size.
   c. Alpha: output the decoded values untouched by default (Alley writes straight colour; the file does not say); a decoder option converts straight -> premultiplied. Expose the variant's alpha presence.
   d. Strict validation: every rejection in the spec's checklist returns an error code; never read or write out of bounds on any input (all lengths checked before use).
   e. Optional parallelism for the HQ planes behind a caller-supplied job callback (no threads created inside the core).
2. C++ header (thin RAII wrapper, header-only) for LoadEhr's C++ FFGL code.
3. SwiftPM package: C target + a small Swift wrapper (DXVKit module) shaped so Glance-style callers can swap in later: open file, frame count / size / variant, decode frame N to BCn buffers or to RGBA.
4. dxvtool CLI: info / dump-blocks / dump-rgba / verify <vectors.tsv> / bench.
5. Universal build (arm64 + x86_64), CMake + SwiftPM, -Wall -Wextra -Werror on our code.

BUILD + TEST
1. Every valid vector: block streams byte-identical to the reference (all 127), RGBA identical to the reference; every invalid vector rejected with a sensible error (all 11). dxvtool verify prints the table.
2. Robustness: AddressSanitizer + UBSan run over all vectors; a libFuzzer (or AFL-style) run on packet decode and on MOV demux, at least 30 min each, 0 crashes / 0 sanitizer reports; corpus = the vectors.
3. Real files: decode every packet of 20 DXV files from ~/Movies (one per variant at least, incl. ~/Movies/Testfiles/Metric_01.mov for legacy; list paths, do not copy them into the repo): 0 errors, every packet consumes its bytes exactly.
4. Speed (name the machine): ms per frame per variant at 1080p and 4K, single thread and with the HQ plane jobs; target = comfortably real time for 4K60 on the M3 / M5 laptop. Report.
5. Commit + push. Handoff docs/handoffs/dxvkit_2026-10-08_w2-cleanroom-results.md, cp -p to ~/claude-handoffs/: API summary, the verify table, sanitizer + fuzz receipts, real-file results, speed, QUESTIONS.md items and how each was settled, the attestation, Amrit's answers from this session.

NOT IN THIS WAVE (named so nobody adds it): the encoder; wiring DXVKit into LoadEhr's projector (next LoadEhr wave); replacing the LGPL files in Glance / GlEnc / CueGlance / MaCrate (one job per app, later).

STANDING RULES
One Claude per folder. Cowork runs no git. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. No third-party footage in the repo. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
