```
DXVKIT W1 — CLEAN-ROOM DXV, PHASE A (THE "DIRTY ROOM"): WRITE THE FORMAT SPEC AND THE TEST VECTORS, NO DECODER CODE — RUN TO FINISHED

MODE: new project. New folder ~/Documents/claude/DXVKit, new private repo AmritusG/dxvkit (main), licence MIT (Amrit Rosell). This session is the SPEC WRITER only. It writes NO decoder or encoder code at all. A different, fresh session implements later from the spec alone (Phase B).
First: create the repo, then cp -p this brief into its docs/handoffs/. Handoffs: dxvkit_<date>_<slug>.md, cp -p to ~/claude-handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"But DXV is Resolume native, why can't it be used inside a plugin that is built specifically for Resolume?!?"
"You do know you yourself managed to build Glance, CueGlance, GlEnc? And you managed to built it BETTER than Resolume?"
"Clean room. Can it be made to work / merge/ replace Glance and the others you built?"

WHY (facts already checked by Cowork)
1. Resolume publishes no DXV spec or SDK; Arena's decoder is not reachable from a plugin.
2. Glance's DXV decoders are faithful ports of FFmpeg libavcodec/dxv.c and carry LGPL-2.1-or-later (glenc-public: DXVPacketDecoder.swift, DXVHQOpcodeDecoder.swift, DXVHQCgoDecoder.swift; encoder side DXVLZWriter / DXVHQOpcodeWriter / DXVHQCgoWriter likewise). DXV1PacketDecoder is BSD-2. LGPL is not on Amrit's licence list.
3. Goal: ONE MIT library (DXVKit) that decodes, then later encodes, every DXV variant, used by LoadEhr (C++ FFGL) AND later replacing the LGPL files in Glance / GlEnc / CueGlance / MaCrate (Swift).

THE CLEAN-ROOM PROTOCOL (binding; write it into DXVKit's CLAUDE.md as HARD RULES)
1. Phase A (this session, dirty room) MAY read: FFmpeg dxv.c / dxvenc.c / texturedsp.c, Glance / GlEnc source, Resolume docs, real DXV files.
2. Phase A writes ONLY: a prose specification + tables, and test vectors. NO source code, NO pseudocode that mirrors upstream structure, NO upstream identifier or function names, no comments copied, no line references. Describe the FORMAT and its BEHAVIOUR: container fields, header bytes, every variant and how it is signalled, every bitstream / opcode / back-reference rule as data layout and meaning, edge cases (padding: DXV1 pads width AND height to 16, DXV3 width only — verify), error conditions. Diagrams and byte tables welcome.
3. Phase B (later, a fresh session after /clear, the clean room) may read ONLY: DXVKit's spec, its test vectors, public BCn (BC1/BC3/BC4/BC5) and HAP / Snappy / LZF documentation, the MOV container spec. It must NOT open FFmpeg, Glance, GlEnc, CueGlance or MaCrate source. Cowork (who has seen only Glance's licence headers, not its code) writes the Phase B brief.
4. Keep a provenance log (docs/cleanroom/PROVENANCE.md): what Phase A read, when, by which session; Phase B will add its own attestation.

READ FIRST (all of it, Phase A only)
1. FFmpeg libavcodec/dxv.c, dxv.h, dxvenc.c, texturedsp.c / texturedspenc.c (the version Glance tracks + current master; note the differences).
2. glenc-public: Sources/GlanceCore/*DXV*, DXV1PacketDecoder, DXVDemuxer, DXVDetector, BC4BC5Unpack, THIRD-PARTY-NOTICES.md, and the Glance memory note: DXV1 / DXDI legacy (4-byte packet header, canonical LZF, DXT1 / DXT5), the padding rule.
3. Every DXV file on this Mac you can find (Resolume's own demo content, Amrit's clips: list paths, do not copy his clips into the repo), and what Alley / GlEnc can export.
4. Apple MOV / QuickTime container docs (where the DXV sample data and the codec FourCC live).

IMPLEMENT (spec + vectors only)
1. docs/spec/DXV_FORMAT.md: overview; container (FourCCs, sample description fields, frame packets); every variant: DXV1 / DXDI legacy, DXV3 normal DXT1, DXV3 normal DXT5 (with alpha), DXV3 HQ (YCoCg, BC4 / BC5-like planes), DXV3 HQ + alpha, any other type tag found; per variant: what the decoded output is (which BCn block stream(s), plane layout, colour transform to RGB(A) with exact maths), the compression scheme in prose + tables, buffer size rules, what a decoder must reject.
2. docs/spec/ENCODING_NOTES.md: what an encoder must emit to be valid (for a later encoder phase; same no-code rule).
3. test_vectors/: for each variant, small clips (a few frames, small size; generated with GlEnc's glenc-cli or Alley from test images we may commit: our own generated patterns, no third-party footage), plus the REFERENCE OUTPUT per frame = the raw decompressed BCn block stream(s) and the final RGBA, produced by running Glance's existing decoder (outputs only; the tool you use to dump them stays OUT of the repo, in the scratchpad). Include edge cases: odd sizes (non-multiple of 4 / 16), 1-frame, a corrupt / truncated packet with the expected rejection. Manifest with SHA-256 per file. Keep the folder small (< 50 MB).
4. docs/cleanroom/PROVENANCE.md as above. CLAUDE.md with the HARD RULES, the planned C99 core + C++ header + SwiftPM wrapper layout (Phase B builds it), licence MIT.
5. A self-check: grep the spec for upstream identifiers (dxv_, op_, AV_, avctx, bytestream, any FFmpeg function name) = 0 hits; no code blocks except byte tables.

BUILD + TEST
1. Nothing to build. Verify every test vector's reference output reproduces from Glance twice (bit-identical) and that the manifest hashes match.
2. Commit + push. Handoff docs/handoffs/dxvkit_2026-10-08_w1-spec-results.md, cp -p to ~/claude-handoffs/: variants found, which DXV files exist on this Mac, what the spec covers, open unknowns (variants seen nowhere), vector list + sizes, the self-check receipts, Amrit's answers from this session.

STANDING RULES
One Claude per folder (this session owns DXVKit; reads glenc-public / FFmpeg only). Cowork runs no git. Licences MIT / BSD / Apache / zlib / ISC / Boost only for anything committed. No footage that is not ours in the repo. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
