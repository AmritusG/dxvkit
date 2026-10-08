# DXVKit: a clean-room MIT library for Resolume's DXV video codec

Goal: ONE MIT library that decodes (later also encodes) every DXV variant, used by LoadEhr (C++ FFGL) and
later replacing the LGPL-2.1 DXV files in Glance / GlEnc / CueGlance / MaCrate (Swift). Licence: MIT,
Amrit Rosell (`LICENSE`).

## HARD RULES: the clean-room protocol (binding on every session in this folder)

1. **Two rooms.** Phase A (the "dirty room", W1, 2026-10-08, done) MAY read FFmpeg `dxv.c` / `dxvenc.c` /
   `texturedsp*.c`, Glance / GlEnc source, Resolume docs and real DXV files. Phase B (the "clean room")
   implements.
2. **Phase A writes ONLY prose specification, tables and test vectors.** No source code, no pseudocode
   that mirrors upstream structure, no upstream identifiers or function names, no copied comments, no
   upstream line references. Byte tables and diagrams are fine.
3. **Phase B MAY read ONLY:** this repository's `docs/spec/`, `test_vectors/`, `docs/cleanroom/`, this
   file and `README.md`; public BC1 / BC3 / BC4 / BC5 (S3TC / RGTC) documentation; public HAP / Snappy /
   LZF documentation; public FSE / tANS documentation (Yann Collet's FSE docs, RFC 8878); the QuickTime /
   MOV container specification. **Phase B MUST NOT open FFmpeg, Glance, GlEnc, CueGlance or MaCrate source
   code** (including the vendored GlanceCore copies, ports or forks of them), and must not ask another
   session to summarise them. Phase B starts in a fresh session after /clear.
4. **Provenance.** `docs/cleanroom/PROVENANCE.md` records what each phase read, when and in which session.
   Phase B appends its own attestation; nobody edits another phase's entry. If a Phase B session finds
   the spec wrong or incomplete, it writes the question into a handoff for Amrit / a dirty-room session
   instead of looking at upstream code.
5. **Licences:** only MIT / BSD / Apache / zlib / ISC / Boost for anything committed. Nothing LGPL / GPL,
   not even as a test dependency.
6. **No footage that is not ours.** Test media = our own generated patterns or synthetic test patterns
   (FFmpeg `testsrc2`). Never copy Amrit's clips or Resolume's demo content into the repo; list paths
   instead.

## Layout

```
docs/spec/DXV_FORMAT.md      the format specification (Phase A)
docs/spec/ENCODING_NOTES.md  what an encoder must emit (Phase A)
docs/cleanroom/PROVENANCE.md who read what
docs/cleanroom/INVENTORY.md  DXV files on the reference machine (paths by folder, no clips)
docs/handoffs/               dxvkit_<date>_<slug>.md, cp -p to ~/claude-handoffs/
test_vectors/                streams/{resolume,glenc,synthetic,invalid}, reference/, source/, vectors.tsv, MANIFEST.sha256
```

Planned for Phase B (not built yet):

```
src/        C99 core, names of Phase B's own choosing. Container-agnostic packet decoder: packet bytes + display size in, block stream(s) out;
            optional CPU BCn -> RGBA. No allocation inside the hot path (caller-provided buffers).
include/    C header + a thin C++ header (RAII wrapper) for LoadEhr / FFGL.
swift/      SwiftPM package wrapping the C core (Glance / GlEnc / CueGlance / MaCrate).
tools/      vector runner: decodes every test_vectors row, compares block streams (exact) and RGBA.
```

Phase B gates: every `decode` row of `test_vectors/vectors.tsv` matches its block streams byte for byte;
every `reject` row is rejected without crashing (run under AddressSanitizer + UBSan); a fuzz pass over the
vectors' packets with random corruption (no crash, no out-of-bounds read).

## Facts worth knowing before anything else (details in the spec)

- Both dimensions pad to 16 in every variant (not "width only").
- The HQ opcode streams of real Resolume files are FSE-coded and use all 18 opcodes; only the
  `resolume` vectors cover that.
- Alpha files exist in both straight (Alley) and premultiplied-looking (Adobe Media Encoder plugin)
  forms; the file does not say which.

## Standing rules

One Claude per folder. Cowork runs no git. Copies preserve timestamps (`cp -p`). Anything Amrit must do:
WHAT / HOW / WHERE. Handoffs per `~/.claude/CLAUDE.md`, plus the in-repo copy in `docs/handoffs/`.
