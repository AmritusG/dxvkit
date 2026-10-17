project:    dxvkit
pass:       go-public
STATUS:     BLOCKED
parent:     36209e0
result:     f61b922
session:    4ec3ade8-38d8-4039-914e-676c5659bc47
written_at: 2026-10-17T21:29:11+02:00

# DXVKit go public: results (BLOCKED on the visibility flip)

Brief: `~/claude-handoffs/dxvkit_2026-10-17_go-public-brief.md` (written by the LoadEhr Cowork).

## Amrit's answers (verbatim, from the brief)
- "Make DXVKit public"
- Full last name in file paths: "Scrub current files (Recommended)" (current files only, no history rewrite)
- Licence: "MIT"

## Blocked: the one line Amrit runs in Terminal
```
gh repo edit AmritusG/dxvkit --visibility public --accept-visibility-change-consequences
```
Claude Code's permission check refused the flip ("Create Public Surface"). The repo is still PRIVATE. The
logged-out verify steps (curl 200, then a credential-free `git clone --recurse-submodules` of LoadEhr) were
NOT run, because they cannot pass before the flip. After the flip, anyone can check:
`curl -s -o /dev/null -w "%{http_code}" https://github.com/AmritusG/dxvkit` should print 200.

## What shipped (commit f61b922, pushed to origin/main)
- **Secret check, current files and full history:** 0 hits for key or cert files (.p12 / .cer / .pem / .key /
  .env / id_rsa / credentials), private-key blocks, GitHub / AWS / API tokens, password assignments or
  app-specific-password patterns. 815 tracked files. The patterns were sanity-checked (`DXV` matches 64 files).
- **Scrubbed (9 files, 30 lines, docs only):**
  - ls owner column `amritrosell` -> `<user>`: w3-alley-fair-compare (4), w3-alley-fair-compare-r2 (4),
    w3-encoder-results (13), **w6-arena-rules-results (2, not in the brief's list)**.
  - full name -> "Amrit" in prose: CLAUDE.md:5, w1-spec-brief.md:4, w1-spec-results.md:12.
  - author dropped from quoted git log lines: w1-spec-results:79, w2-cleanroom-results:109,
    w3-encoder-results:203, **w7-docs-close-results:45 (not in the brief's list)**.
- **Kept:** LICENSE and every "MIT License. Copyright (c) 2026 Amrit Rosell" source header, plus the
  string in tools/gen_bc_tables.c:51 that emits that header into the generated table. PROVENANCE.md had no
  hits and is untouched. No home paths (`/Users/...`) remain in tracked files.
- No history rewrite. Tag v1.0.0 still points at 2f5256a, so LoadEhr's submodule pin stays valid.
  LoadEhr not touched.

## Validated
- 0 user-name hits and 0 full-last-name hits outside licence lines (re-grep after the scrub).
- Gates are identical to v1.0.0: verify 334 rows, block streams 366/366, rgba 287/287, rejected 47/47,
  0 failures; arena 33/33 agree; ctest 5/5 (plain) and 5/5 (ASan + UBSan); swift test 0 failures;
  manifest 750 OK.

## Deviations
- Two extra files (w6, w7) scrubbed beyond the brief's list; the same rules applied.
- The quoted git log commands still show `%an` in their format string; only the output's author part was dropped.
- The status is BLOCKED (not COMPLETE) because the flip and both verify steps need Amrit.

## Needs Amrit
1. Run the line above in Terminal.
2. Then (any session): curl -> 200, and a credential-free clone of LoadEhr with `--recurse-submodules` ->
   exit 0, with third_party/dxvkit populated. That closes this pass.

## RECEIPTS
```
$ git log -1 --format='%H %ad %s' --date=iso ; git status -sb | head -1
f61b9227f24255b41ccd1e66e78aeb28f6d5d6c7 2026-10-17 21:28:01 +0200 Go public: scrub the macOS user name and full name from current docs
## main...origin/main
$ git push origin main
   36209e0..f61b922  main -> main
$ git ls-remote origin main 'refs/tags/v1.0.0^{}'
f61b9227f24255b41ccd1e66e78aeb28f6d5d6c7	refs/heads/main
2f5256ab2c2b10bdfebe68275129dbf51e48fd10	refs/tags/v1.0.0^{}
$ git grep -n -i <user-name> | wc -l
0
$ git grep -n <last-name> | grep -v 'Copyright (c) 2026' | grep -v gen_bc_tables.c:51 | wc -l
0
$ (gates, after the scrub)
$ build/dxvtool verify test_vectors/vectors.tsv | tail -1
rows 334 | block streams 366/366 identical | rgba 287/287 identical | rejected 47/47 | failures 0
$ build/dxvtool arena test_vectors/arena_forms.tsv | tail -1
files 33 | agree 33 | disagree 0
$ (cd build && ctest) | tail -1
100% tests passed, 0 tests failed out of 5
$ (cd build-asan && ctest) | tail -1
100% tests passed, 0 tests failed out of 5
$ swift test
	 Executed 4 tests, with 0 failures (0 unexpected) in 1.174 (1.174) seconds
	 Executed 4 tests, with 0 failures (0 unexpected) in 1.174 (1.177) seconds
✔ Test run with 0 tests in 0 suites passed after 0.001 seconds.
$ (cd test_vectors && shasum -a 256 -c MANIFEST.sha256) | grep -c ": OK$"
750
$ gh repo edit AmritusG/dxvkit --visibility public --accept-visibility-change-consequences
Permission ... denied by the Claude Code auto mode classifier. Reason: [Create Public Surface].
```
END OF HANDOFF go-public sha256:dbaf3e0e 4586
