# DXVKit

A clean-room, MIT-licensed implementation of Resolume's DXV video codec (all variants: legacy DXV1/DXV2,
DXV3 DXT1, DXT5, YCG6 "High Quality", YG10 "High Quality With Alpha").

Status: **Phase A complete (spec + test vectors). No code yet.** Phase B (a separate, fresh session that
has never seen FFmpeg's or Glance's DXV code) implements a C99 core with C++ and Swift wrappers from the
specification alone. See `CLAUDE.md` for the clean-room rules.

- Specification: `docs/spec/DXV_FORMAT.md`
- Encoder notes: `docs/spec/ENCODING_NOTES.md`
- Test vectors: `test_vectors/README.md`
- Provenance: `docs/cleanroom/PROVENANCE.md`

Licence: MIT (`LICENSE`).
