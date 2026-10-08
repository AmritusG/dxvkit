```
DXVKIT W3 — CLEAN-ROOM DXV ENCODER (CLEAN ROOM, LIKE THE DECODER): EVERY DXV3 VARIANT, CHECKED BY OUR DECODER, BY FFMPEG AS A BLACK BOX AND BY ARENA — RUN TO FINISHED

MODE: build wave on DXVKit (~/Documents/claude/DXVKit, AmritusG/dxvkit). THIS MUST BE A FRESH SESSION: Amrit runs /clear (or opens a new Claude Code in the DXVKit folder) before pasting this. If this session's context holds anything from FFmpeg, Glance or GlEnc source, STOP and say so.
Runs IN PARALLEL with the Glance session (another folder; Glance pins DXVKit at cdbb4bc, so new commits here do not touch it) and possibly the LoadEhr session (another folder). Never write outside the DXVKit folder (except cp -p of the handoff to ~/claude-handoffs/).
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"Clean room. Can it be made to work / merge/ replace Glance and the others you built?"
"Since the encoder is separate, can it run now while I update Glance?"

WHY
GlEnc's DXV writers (DXVLZWriter, DXVHQOpcodeWriter, DXVHQCgoWriter) are LGPL ports / derivations of FFmpeg. DXVKit's encoder replaces them so GlEnc can drop LGPL too (that swap is a later GlEnc job, not this one).

THE CLEAN ROOM (binding; DXVKit CLAUDE.md HARD RULES apply unchanged)
1. You MAY read: this repo (CLAUDE.md, README, docs/spec/DXV_FORMAT.md, docs/spec/ENCODING_NOTES.md, docs/design.md, docs/cleanroom/, docs/handoffs/, test_vectors/, and DXVKit's own decoder source in src/ + include/: it is clean-room MIT code from W2); public BC1 / BC3 / BC4 / BC5 docs; RFC 8878 + Yann Collet's FSE / tANS write-ups; the QuickTime File Format spec; the LZF format description.
2. You MUST NOT open, search or fetch FFmpeg source (dxvenc.c, dxv.c, texturedspenc.c, any mirror or doxygen page), Glance, glenc-public, GlEnc, CueGlance, MaCrate, Alley internals, or any other DXV implementation.
3. BLACK-BOX USE IS ALLOWED (outputs only, never source): the ffmpeg / ffprobe BINARIES already on this Mac as a decoder oracle (decode our files to RGBA and compare), and Resolume Arena / Alley loading our files. Record each black-box use in PROVENANCE.md.
4. Vendored third-party code: only MIT / BSD / Apache / zlib / ISC / Boost / public domain and never a DXV implementation. A BCn block encoder library is allowed (e.g. a permissive BC1 / BC3 / BC4 / BC5 encoder): name it, its licence and why, or write your own from the public BCn docs. FSE: your own from RFC 8878, or a permissive FSE library (name + licence).
5. Spec silent or ambiguous: write it into docs/cleanroom/QUESTIONS.md, settle it ONLY by experiment against our decoder, the vectors and the black-box oracles, record how. Unresolvable items go to Amrit / a dirty-room session via the handoff.
6. Append your attestation to PROVENANCE.md at the end (session id, what you read, black-box uses, "did not access" list, any accidental exposure).

READ FIRST (all of it; extract into docs/design-encoder.md before writing code)
1. ENCODING_NOTES.md and DXV_FORMAT.md in full; docs/design.md; QUESTIONS.md (W2's 9 items: the dangling HQ opcode tail, FSE spread step 641, selector 3 never seen, literals past the opcode-area offset rejected, etc.); the W1 + W2 results handoffs.
2. The container section: sample-entry fields as Resolume writes them (FourCC DXD3, the per-variant tags), the 12-byte DXV3 header, 16x16 padding on BOTH axes.
3. Public BCn docs for every block type you emit; RFC 8878's FSE section for the HQ opcode streams.

IMPLEMENT (C99 core, MIT headers, libc only plus any permissive vendored encoder you name; same style as the decoder: no globals, caller buffers, jobs callback for parallelism)
1. ENCODE API: RGBA8 (straight alpha by default; an option says the input is premultiplied and is written as such) -> packet, for DXT1, DXT5, YCG6, YG10. Also a blocks-in path (caller-supplied BC1 / BC3 / BC4 / BC5 planes -> packet) so a caller with its own BCn encoder can use only the DXV packing. Odd sizes padded to 16 x 16 by edge replication.
2. PACKING: the DXV3 compression of each variant as the spec describes (DXT1 / DXT5 back-reference layer; HQ: YCoCg forward transform with the exact maths the decoder inverts, BC4 / BC5 planes, opcode streams). HQ opcode streams: implement FSE-coded streams (what Resolume writes) AND stored / fill; an encoder option picks, default = whichever the measurements below show matches Alley's size better. Never emit what the spec says a decoder must reject; avoid features never seen in real files (selector 3, the stored flag) unless an option asks for them.
3. EFFORT / QUALITY: at least two effort levels (fast for live / batch, best for archive) for the BCn search and the match search; state what each changes.
4. MOV WRITER (video only): write a playable .mov with a DXD3 track exactly as the spec's container section describes (sample-entry fields, timescale / frame rate, stsz / stco / co64 / stsc), so dxvtool can produce test files. Audio muxing stays the caller's job (GlEnc) and is out of scope.
5. C++ header + Swift API additions (DXVEncoder / DXVEncodeOptions) mirroring the decoder's shape; dxvtool encode <in images or raw RGBA sequence> <out.mov> --variant --effort.

BUILD + TEST
1. ROUND TRIP (all variants, all efforts, sizes 16x16, 18x10, 333x221, 1920x1080, 908x2276, 3840x2160, 1x1): encode -> our decoder -> block streams byte-identical to what the encoder packed (the packing layer is lossless) and the decode consumes every byte exactly (dxvtool check).
2. BLACK-BOX ORACLE: every output file decodes with the ffmpeg binary without warnings, and its RGBA equals our decoder's RGBA (or differs only by the documented BC1 expansion convention: say which).
3. RESOLUME: Arena opens every variant's output (load into clips of a new layer "DXVKit encoder" by the Arena MCP or REST, read back the clip thumbnails / status WITHOUT triggering; never trigger or activate a clip, never kill Arena, never ask Amrit to save the comp); Alley opens them if it can be driven without its GUI, else say so. Say which columns hold which file so Amrit can fire them.
4. VS ALLEY: for the testsrc2 sources the resolume vectors were made from (test_vectors/source + the resolume streams), our output size per variant vs Alley's, and PSNR / max error vs the source RGBA for ours vs Alley's (decoded with our decoder). Report the table; no threshold, Amrit judges.
5. SPEED (name the machine; parallel with the other Claude sessions, so say what else was running): encode fps per variant at 1080p and 4K, single thread and with jobs, per effort level.
6. ROBUSTNESS: ASan + UBSan over all round-trip tests; a libFuzzer run on the encoder with random sizes / random pixels / random options, 30 min, 0 crashes, every output decodes.
7. Decoder unchanged: dxvtool verify on test_vectors still 127/127, 91/91, 11/11 (the encoder must not touch decoder behaviour).
8. Commit + push. Handoff docs/handoffs/dxvkit_2026-10-08_w3-encoder-results.md, cp -p to ~/claude-handoffs/: API summary, round-trip table, oracle results, Arena columns, Alley comparison table, speeds, fuzz / sanitizer receipts, QUESTIONS items + how settled, the attestation, what GlEnc needs to swap its LGPL writers for this (inputs it must provide, options to map), Amrit's answers from this session.

NOT IN THIS WAVE: editing GlEnc or any other app; legacy DXDI writing (nothing writes it); audio muxing.

STANDING RULES
One Claude per folder (this session owns DXVKit only). Cowork runs no git. Licences MIT / BSD / Apache / zlib / ISC / Boost / public domain only for anything committed; nothing LGPL / GPL even as a test dependency (the ffmpeg binary is used as an external black box, never linked or committed). No footage that is not ours in the repo. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
