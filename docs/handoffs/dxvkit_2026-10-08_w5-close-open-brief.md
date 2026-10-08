```
DXVKIT W5 — CLOSE THE LAST OPEN SPEC ITEMS (Q13, SPEC 10.1-10.4, 10.6, 10.7) WITH THE AUTHORITY THAT CAN ANSWER EACH: UPSTREAM SOURCE (DIRTY ROOM), ARENA AS A BLACK BOX, ALLEY EXPORTS — RUN TO FINISHED

MODE: PHASE A (dirty room) wave on DXVKit, docs + test vectors only (DXVKit HARD RULES: prose / tables / vectors, no code in src/ include/ swift/ tools/, no upstream identifiers or line refs in the spec). Fresh session (/clear first). Tooling scripts this session needs stay in the scratchpad, never committed.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"How do we solve the Still open backlog?"
(His rule: a backlog is not finished work: clear it, validated. "Measuring files on this Mac can't settle it" is not a closure: find the authority that can.)

THE ITEMS AND THE AUTHORITY FOR EACH
1. Q13 (ffmpeg's BC1 expansion / rounding). Authority: ffmpeg's own source, which Phase A MAY read. Read texturedsp.c (and the dxv.c call path), state ffmpeg's exact BC1 colour expansion + interpolation rule in prose / a table, confirm it against the W3 oracle data (DXT1 max |d| 1 on 0.6 % of bytes), close Q13. No measuring needed.
2. Spec 10.1-10.4 (legacy DXT5 / stored legacy / other version nibbles / the "nibble 2 = DXT1" rule; DXV3 stored flag on YCG6 / YG10; other version / minor / reserved bytes; other sample-entry FourCCs). No real file exists, so the authority is RESOLUME ARENA AS A BLACK BOX: what Arena does with each form IS the answer for a Resolume-compatible library.
   a. Build one small synthetic file per form (from the existing synthetic vectors / DXVKit's dxvtool + byte edits in scratch; our own pattern), including the negative ones (version 5, minor 1, reserved non-zero, a FourCC like DXD4, HQ with the stored flag).
   b. Load them into a new Arena layer "DXVKit W5 forms" over REST / the Arena MCP, NOT triggered: record per file what Arena reports (type, size), whether its thumbnail decodes, and the log / error.
   c. Then ask Amrit to fire each column (WHAT / HOW / WHERE: layer name, column numbers, "click the clip; tell me which play, which show black or an error") and record his answer verbatim.
   d. Write each result into spec 10.x as "Arena 7.28 behaviour" and set DXVKit's rule to match (accept what Arena plays, reject what it rejects). If DXVKit's decoder disagrees with Arena on any form, that is a FINDING for the next clean-room wave (do not touch code).
3. Spec 10.6 (chroma siting / filtering in Resolume's HQ playback). Authority: Arena's actual output pixels.
   a. Make a YCG6 + YG10 test file with sharp chroma edges (hard colour steps on odd and even pixel columns / rows, a 1-pixel colour grid, a colour-only checkerboard on flat luma) with DXVKit's encoder.
   b. Capture Arena's composition output pixel-exact: a tiny Syphon client (Syphon framework, BSD; scratchpad only) or Arena's own recorder if it writes lossless; composition at the file's exact size, no scaling, layer opacity 1, no effects. Amrit fires the clip (Claude never triggers); ask him with WHAT / HOW / WHERE.
   c. Compare the captured frame with DXVKit's decode under candidate models (nearest, bilinear on centred / co-sited chroma, Arena-side scaling) and state which one Arena uses, with the error numbers. Write it into 10.6. If LoadEhr's projector (nearest, W3) or Glance differ from Arena, list it as a finding for those projects.
4. Spec 10.7 (Resolume's forward YCoCg transform + its BC encoders). Authority: Alley's output on sources we control.
   a. Make lossless sources (qtrle or PNG sequence, 1920x1080, a few frames): flat colour patches covering the RGB cube corners and mid-greys, horizontal / vertical ramps per channel, a chroma checkerboard, the W3 testsrc2 source already used.
   b. Ask Amrit to export them through Alley in YCG6 and YG10 (WHAT: Alley; HOW: drag the file in, preset "High Quality No Alpha" and "High Quality With Alpha", Convert; WHERE: write to ~/Movies/Testfiles/dxvkit-w5-alley/ and name them so names match contents), then WAIT and check the names with dxvtool info.
   c. From Alley's decoded planes vs the known source, derive the forward transform exactly (rounding of Y / Co / Cg, chroma subsampling filter and siting, the +128 offset, clamping), and characterise Alley's BC4 / BC5 endpoint choices only as far as it matters (it does not have to be matched: W3 already showed size / quality parity). Write it into 10.7 and ENCODING_NOTES. If DXVKit's encoder transform differs in a way that costs quality or compatibility, that is a FINDING for the clean room.
5. After this, spec section 10 must list NOTHING open, or each remaining line must name the evidence that it cannot ever be settled (for example: a form Arena itself refuses and no writer produces = defined as invalid). "Unobserved" alone is not a closure.

BUILD + TEST
1. Nothing to build in the library. dxvtool verify still 127/127, 91/91, 11/11; MANIFEST updated for any new vectors (small, our own patterns).
2. Self-check: upstream identifier grep on docs/spec = 0 hits, no code fences, no line refs (receipt).
3. Commit + push. Handoff docs/handoffs/dxvkit_2026-10-08_w5-close-open-results.md, cp -p to ~/claude-handoffs/: each item, its authority, the evidence, the spec change, findings for the clean room / LoadEhr / Glance, Arena layer + columns used, Amrit's answers verbatim.

STANDING RULES
One Claude per folder (nothing else in DXVKit while this runs). Cowork runs no git. Never trigger or activate clips (Amrit fires them); never kill Arena; do not ask Amrit to save the comp. No footage that is not ours in the repo (Alley exports of OUR sources may be committed if small; list their paths otherwise). Licences MIT / BSD / Apache / zlib / ISC / Boost only for anything committed. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
