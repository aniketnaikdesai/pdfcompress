# Plan Notes — PDF Compressor Phase 0

- Issue: RG-001

**Requirement/Journey:** AC3 — Corpus generator — 14 deterministic types (QPDF-only, `$TMPDIR/pdfcompress_test_corpus`, encrypted `test123`) — file count / naming

**Problem:** AC3 lists 14 enumerated items where #14 "covers both alpha and duplicate-font" yet also says "generate both `transparency.pdf` and `merged_duplicate_fonts.pdf` and treat the latter as the 14th file" and later "alternatively generate 15 files and document…". Strictly reading both clauses, the required file count (14 vs 15) and file naming cannot be satisfied simultaneously without choosing an interpretation.

**Why it cannot be satisfied as currently written:** A corpus that generates both `transparency.pdf` and `merged_duplicate_fonts.pdf` as separate files contains 15 logical types if `photo_jpeg` + `photo_heavy` and `bookmarks_and_links` + `javascript` are already counted separately; but `ls *.pdf | wc -l == 14` would then fail. Conversely, generating exactly 14 files requires folding the merged-duplicate-fonts coverage into one of the other 14.

**Suggested Direction:** Lock exactly 14 files as the single authoritative count (AC3 1–14 list). Implement `transparency.pdf` so it contains both an `/SMask` or `/Group /Transparency` alpha case **and** a shared indirect XObject referenced on two pages (same `QPDFObjectHandle`), thereby covering both "transparency" and "merged duplicate fonts" in one file. Do **not** emit a 15th `merged_duplicate_fonts.pdf` unless Task Manager explicitly documents a 15-file deviation and updates the verifier test count. Verifier test `TEST(CorpusGenerator, FileCountIs14)` must assert `==14`, and per-file key checks must assert both SMask/transparency and shared-object presence on that single file.

**Status:** open

**Escalation:** none — interpretation locked by Validator as the `Status: READY_FOR_TASK_MANAGER` baseline. If the product owner requires 15 files instead, file a follow-up requirement change; no `ESCALATED_TO_REQUIREMENTS` is emitted for Phase 0.

---

- Issue: RG-002 (informational — no escalation)

**Requirement/Journey:** AC3 `monochrome_bw.pdf` "1-bit concept" via DeviceGray 8-bit; AC7(d) SSIM≥0.98 on synthetic corpus; AC3 `generateCmykImage` DCTDecode vs FlateDecode; AC5 `git diff --stat` ±1% on tiny PDFs

**Problem:** These are the proposal's F2–F5. Each is a correctly flagged implementation note / risk, not a requirements defect that blocks Task Manager: (F2) 8-bit grayscale with ≤4 colors is an acceptable "1-bit concept" trigger for Monochrome→zlib; true 1-bit `/BitsPerComponent 1` bit-packing is optional. (F3) Synthetic flat images make SSIM≥0.98 trivially pass — Phase 0 keeps the threshold as a CI gate but notes real gating needs photo-heavy content (Phase D+). (F4) Fixing CMYK to FlateDecode validates the `channels==4` skip path without needing valid JPEG bytes. (F5) Writer overhead on `<2KB` PDFs makes ±1% unattainable; corpus will use ≥50KB realistic streams and the AC5 gate is evaluated on `photo_heavy`/`line_art` sized PDFs, exempting `<2KB` inputs.

**Why it cannot be satisfied as currently written:** N/A — all are satisfiable as written once the mitigation in `docs/architecture.md` / `docs/plan.md` Phase 0.1–0.4 is applied.

**Suggested Direction:** Retain mitigations as documented in `docs/architecture.md` Architecture Risks and `docs/plan.md` Phases 0.1/0.2. No code change outside `tests/`, `benchmark/`, `tools/render_diff.cpp`, `CMakeLists.txt`.

**Status:** open

