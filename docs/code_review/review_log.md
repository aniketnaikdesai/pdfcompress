# Code Review Log — Phase 0, Waves 1–4 (2026-10-04)

Scope: `docs/build/build_log.md` (Waves 1 CONSOLIDATED → 4, `Status: NEEDS_CODE_REVIEW`),
`docs/build/failures.json` (1 blocked: T05-test-verifier / T05-T04),
`docs/build/logs/*.md` (T01–T05), `docs/tasks/task-graph.json`,
`docs/requirements/requirements.md`, `docs/architecture.md`, `docs/plan.md`,
changed files (`tests/test_corpus_generator.*`, `tests/test_corpus_verifier.cpp`,
`benchmark/run_benchmark.sh`, `tools/render_diff.cpp`, `CMakeLists.txt`,
`src/core/PDFOptimizer.cpp` read-only for diagnosis).

Graphify note: `graphify-out/graph.json` does not exist in this repo (verified via
`ls`; worker logs for T01–T05 report the same). Per the skill fast-path/fallback rule
there was no graph to query, so this review used manual inspection
(read/grep/shell verification of sources, scripts, and worker logs). Stated explicitly
per skill rules — not silently skipped.

No prior `docs/code_review/escalations.md` exists — no open entries to close out.

## Entry A — Failure diagnosis (blocked task)

- id: F-001
- linked task: T05-test-verifier (status `blocked`; blocks T06-ctest-wiring-gate via DEPENDS_ON)
- failing test: T05-T04 → GTest `CorpusGenerator.StrippingRegressionSizeGateWithExemption`
- category: bug
- root-cause classification: `code_bug` (implementation does not satisfy acceptance criteria; NOT a task/architecture/requirements problem — see analysis below)
- severity: blocking
- description: Optimizing `transparency.pdf` with the Balanced profile returns
  `success=false` with `QPDF::replaceObject called with indirect object handle`,
  thrown inside the stream-dedup loop in `src/core/PDFOptimizer.cpp` line 367:
  `pdf.replaceObject(obj.getObjGen(), it->second)`. The `it->second` replacement
  handle is an indirect object (the loop only stores indirect streams — line 338
  `if (!obj.isStream() || !obj.isIndirect()) continue;`), but QPDF requires the
  replacement argument to `replaceObject` to be a direct object. The new
  transparency fixture (2 pages with identical `/Contents` streams, plus shared
  image/SMask streams) is the first corpus PDF to contain a byte-identical
  duplicate stream pair, so it is the first to trip this pre-existing bug —
  13/13 other corpus files optimize fine, per worker probe.
- evidence:
  - Reproduction path (worker, trusted and code-confirmed): fresh-built
    `pdfcompress_core` + public `PDFOptimizer::optimize` API on generated
    `transparency.pdf` → `result.success=false`, `errorMessage` carries the
    `replaceObject` complaint. Reviewer confirmed the mechanism by reading
    `src/core/PDFOptimizer.cpp:334-372`: `seenStreams[sig] = obj` stores the
    indirect handle (line 361), and line 367 passes it back as the replacement.
    The `objGen !=` guard (line 366) does not help — both handles are indirect.
    Likely colliding pair is the two identical page `/Contents` streams (same
    bytes, no `/Subtype`/`/Width` keys → signature is size+hash only), not the
    shared image XObject itself (same objGen would be skipped); worker's
    "shared-XObject" phrasing is imprecise but the loop/line and fix owner are right.
  - Requirement in conflict: AC7 — "Given any corpus PDF … When optimized with any
    profile → Then (a) `QPDF().processFile(out)` succeeds" and success is expected;
    T05 acceptance criterion 4 requires the Balanced stripping/size gate to pass.
    The verifier is correct to assert `EXPECT_TRUE(result.success)` in
    `optimizeBalancedChecked` (`tests/test_corpus_verifier.cpp:371-383`) — the
    worker correctly kept the assertion intact rather than weakening it.
  - Why not another classification: task split is fine (T05 touches only the
    verifier file and correctly refused to fix product code outside its footprint);
    architecture is fine (dedup-by-hash design per Appendix A claim 11 is sound —
    only the QPDF call misuses an indirect handle); requirements are consistent
    (AC7 demands success; the Constraints "Resilience" rule — one failure never
    fails the whole file, fall back and log — supports fixing, not exempting).
    AC5's "no changes to `PDFOptimizer.cpp` decision logic" yields here: this is a
    minimal crash fix restoring AC7, not a compression-decision change.
- suggested direction (non-binding): in the dedup loop, pass a direct object as the
  replacement (e.g. a shallow/direct copy of `it->second`) or skip pairs whose
  replacement handle is indirect; keep the `objGen !=` guard. Owned by whoever owns
  `src/core/PDFOptimizer.cpp`.
- routed: ESC-001 → BUILD (`Status: ESCALATED_TO_BUILD`).

## Entry B — Quality review (done tasks T01–T04)

### B1 — T01-corpus-hardening (done, 13/13 pass) — one should-fix finding
- id: F-002
- linked task: T01-corpus-hardening
- category: gap (requirements-coverage gap the tests did not catch)
- root-cause classification: `code_bug` (generated artifact does not match AC3)
- severity: should-fix
- description: `photo_jpeg.pdf` is generated with `/FlateDecode` (see
  `tests/test_corpus_generator.cpp:98` → `createPdfWithImage(..., "/FlateDecode")`),
  but requirements AC3 item 2 normatively specifies `photo_jpeg.pdf` as "200×150
  DeviceRGB DCTDecode (simulated single photo) — exercises JPEG path". With both
  `photo_jpeg` and `photo_heavy` using Flate, no corpus PDF exercises the
  optimizer's `/DCTDecode` → TurboJPEG decode branch (`PDFOptimizer.cpp:244-258`),
  so that branch is untested by the corpus. No T01 test asserts the photo filter,
  which is why it passed vacuously. (Worker rationale — Flate avoids a
  `qpdf --check` warning on invalid DCT bytes — is understood; the correct fix is
  valid JPEG bytes, not a filter swap.)
- evidence: `grep -n DCTDecode tests/test_corpus_generator.cpp` returns only the
  CMYK "not DCTDecode" comment context; AC3 line 102 (`requirements.md`) requires
  DCTDecode for photo_jpeg; optimizer DCT branch at `PDFOptimizer.cpp:244`.
- suggested direction (non-binding): generate real JPEG bytes for the 200×150 photo
  (e.g. via TurboJPEG compress in the generator, mirroring `compress()` use for
  Flate) and keep `/DCTDecode`; or document a locked deviation if JPEG bytes are
  infeasible — do not silently keep Flate.
- routed: ESC-002 → BUILD (`Status: ESCALATED_TO_BUILD`).
- Otherwise T01 verified clean: 14-file invariant holds, `generateAll` uses
  `create_directories`, deterministic IDs per generator (10× `setDeterministicID`
  + static ID/IV for encrypted), Helvetica Type1, `$TMPDIR` storage, per-type keys
  (AcroForm/Widget, Info/Metadata/PieceInfo/Thumb, Names+JavaScript/OpenAction,
  Outlines/Annots/Dests, SMask+shared objGen+Group/Transparency, R6/test123,
  DeviceCMYK+Flate). `large_uncompressed` on-demand (not in the canonical 14) is
  consistent with AC3's 14-type list and T05 regenerates it on demand — no finding.

### B2 — T02-benchmark-hardening (done, 11/11 pass) — no findings
- Verified: `set -euo pipefail`, `BUILD_DIR` default+dir check, corpus regen +
  `Corpus generated at:` parse with fallback, valid `qpdf --recompress
  --compression-level=9 --object-streams=generate` (no `--optimize`), gs
  ebook/screen + ocrmypdf with `command -v` guards → `SKIPPED (not installed)`,
  `run_optimize … || true` with `${pdf%.pdf}_optimized.pdf` outputs, ms timing
  (`%s%3N`/gdate/fallback), portable `stat`, `awk` reduction, exact 9-col TSV
  header with SSIM/PSNR, all five skip patterns, dual TSV + `column -t`.
  Precise `grep -in "python3|scikit|pip install|ImageMagick"` → CLEAN (the only
  `pip` substring in the script is `pipefail`). Offline, no Python. No finding.

### B3 — T03-render-diff-source (done, 5/5 pass) — no findings
- Verified: `<fpdfview.h>` + `FPDF_InitLibraryWithConfig`, LoadDocument/LoadPage(0),
  GetPageWidth/Height → 150 DPI sizing, BGRA `FPDFBitmap_Create` + white FillRect +
  RenderPageBitmap + GetBuffer, RAII `unique_ptr` deleters + library guard,
  `<cmath>`/stride via `FPDFBitmap_GetStride`, C1=6.5025/C2=58.5225 11×11 SSIM,
  PSNR INF on MSE==0, `N/A N/A` exit-0 fallback, no python refs, `-Wall -Wextra
  -Wpedantic` clean for the TU. The out-of-footprint `CMakeLists.txt` edit was
  flagged by the worker and reconciled/retained by T04 (verified present,
  lines 115–121) — observed and resolved, not re-escalated. No finding.

### B4 — T04-render-diff-build (done, 5/5 pass) — one nice-to-have finding
- id: F-003
- linked task: T04-render-diff-build
- category: quality (repo hygiene)
- root-cause classification: `code_bug` (byproduct handling; trivially fixable by Build)
- severity: nice-to-have
- description: Test-run byproducts `benchmark_results_202*.tsv` (repo root and
  `build/` copies) plus `corpus_info*.txt` were left untracked in the working tree
  (`git status` shows 4 TSVs + corpus_info files as `??`). Harmless, but they will
  confuse the T06 `git diff --stat` gate and TSV discovery (newest-file scan).
- evidence: `git status --short` lists `?? benchmark_results_20261003_*.tsv`,
  `?? corpus_info.txt`, `?? corpus_info2.txt`; T04 log acknowledges leaving them.
- suggested direction (non-binding): delete the stray TSVs/txts or add
  `benchmark_results_*.tsv` + `corpus_info*.txt` to `.gitignore`.
- routed: ESC-003 → BUILD (`Status: ESCALATED_TO_BUILD`).
- Otherwise T04 verified clean: `add_executable(render_diff …)` linked
  `PRIVATE pdfcompress_core pdfium` with matching warning flags, guarded
  `[ -x "$BUILD_DIR/render_diff" ]` invocation with `|| echo "N/A N/A"` parse,
  `# render-diff unavailable (PDFium C++ only)` header note, numeric SSIM/PSNR
  verified in TSV. Interop with T05 checked: T05's TSV parser skips the `#` note
  line (`ssimIdx >= cells.size()` → continue) and treats N/A as allowed — compatible.

### B5 — T05 verifier file itself (blocked by product bug, not by its own code) — no findings
- Verified: 21 `TEST(CorpusGenerator, …)` cases, GTest-only, RAII (`unique_ptr<QPDF>`
  in `openValidPdf`), canonical-14 counting robust to shared-dir benchmark outputs,
  per-type `hasKey`/validity assertions, CMYK `imagesSkipped==1`, encrypted
  fail-safe + with-password-unencrypted paths, RG-002 named constants
  (2KB exempt / 50KB ±1%), TSV header + ≥0.98 threshold with N/A tolerance,
  `GTEST_SKIP` when no TSV, no python refs. The file is correct as written and
  should pass unmodified once F-001 is fixed — no escalation against T05.

## Pre-existing tree state (observation, not a finding)
- `git status` shows uncommitted modifications to `src/core/PDFOptimizer.*`,
  `src/main.cpp`, `tools/run_optimize.cpp`, `vcpkg.json` (+`gtest`, required by AC2)
  from parallel/in-flight work outside T01–T05 footprints (also noted in T01/T04/T05
  worker logs). Per review scope (only what `build_log.md`/`failures.json` reflect)
  these are not reviewed here; flagging for the Build Lead because the future
  T06 `git diff --stat` gate (AC5) will need reconciliation once the F-001
  optimizer fix lands anyway.

## Re-review — Wave 5 rework + Wave 6 empty (2026-10-04)

Scope: `docs/build/build_log.md` (Waves 5 CONSOLIDATED + 6, `Status: NEEDS_CODE_REVIEW`),
`docs/build/failures.json` (1 blocked: T05-test-verifier / T05-T04),
`docs/build/logs/T01-corpus-hardening.md` + `T04-render-diff-build.md` + `T05-test-verifier.md`
(wave 5 rework sections), `docs/tasks/task-graph.json` (all 6 tasks' `touches_files`),
prior entries F-001/F-002/F-003 and ESC-001/ESC-002/ESC-003 above.

Graphify note: skill loaded first per protocol; `graphify-out/graph.json` does not exist
in this repo (verified via `ls`; all three worker logs report the same). No graph to
query — manual inspection used (source grep, `qpdf --json`/`--check`, live
`run_optimize` runs, `git check-ignore`/`git status`). Stated explicitly, not skipped.

### R-001 — ESC-001 NOT resolved, stays open (T05 still blocked)
- Linked task: T05-test-verifier (`blocked`); failing test T05-T04 unchanged.
- Why the fix didn't hold: the fix was never attempted, not attempted-and-wrong.
  T05's footprint is `tests/test_corpus_verifier.cpp` only; the crash lives in
  `src/core/PDFOptimizer.cpp:367` (`pdf.replaceObject(obj.getObjGen(), it->second)`
  with indirect `it->second`, stored line 361). The wave-5 worker correctly made no
  product edit (twice now) and left the assertion intact. Reviewer reproduced the
  failure independently this pass: `run_optimize transparency.pdf` prints
  `Optimization failed: QPDF::replaceObject called with indirect object handle`,
  and the dedup loop source is byte-identical to the F-001 diagnosis (indirect
  store + indirect replace + ineffective `objGen !=` guard). `failures.json` and
  wave-6 frontier both still name T05-T04.
- Classification unchanged: `code_bug`, blocking. No new architecture/requirements
  defect: dedup-by-hash design is sound, AC7 still demands success, AC5's
  "no decision-logic changes" yields to a minimal crash fix (per F-001).
- Action: ESC-001 left `open` (still `ESCALATED_TO_BUILD` — Build is the eventual
  fixer). Because no task owns the file (see R-004), a second escalation ESC-004
  routes the task-split half to TASK_MANAGER. Cross-reference: ESC-001.
- No escalation against T05's verifier file: still correct as written (21 TESTs,
  RG-002 gates, N/A-tolerant TSV); T05-T01's `PASS-WITH-NOTE` (strict
  `hasKey.*JavaScript` grep vs the file's `/Names`+`find("JavaScript")` impl, live
  test passing) is a test-wording nicety only — observation, not a finding.

### R-002 — ESC-002 verified fixed, closed (T01 rework quality: clean)
- Re-verified by reviewer (not taken on trust): `grep` shows `turbojpeg.h` /
  `tjCompress2` in `tests/test_corpus_generator.cpp`; live `qpdf --json
  photo_jpeg.pdf` contains `DCTDecode` + `DeviceRGB`; raw bytes contain `FFD8FF`;
  `qpdf --check` clean; reviewer-ran `run_optimize photo_jpeg.pdf` reports
  `Optimization Successful (1 optimized)` — the `/DCTDecode` TurboJPEG branch
  (`PDFOptimizer.cpp:244-258`) is now exercised. Failure mode is loud (`throw` on
  TurboJPEG failure, no silent Flate fallback). `generateAll` still emits exactly
  the canonical 14; all T01-T01..T13 re-run PASS per wave-5 log.
- Out-of-scope observation (not fixed, correctly left alone): full `ctest` shows 3
  pre-existing failures in `tests/test_pdf_optimizer.cpp` (`BookmarksStripped`,
  `AnnotationsStripped`, `CmykSkipped`) — a different file outside T01's footprint,
  failing independently of the photo change. Logged as a note for the T06 gate,
  not an escalation: those cases belong to no task's `tests` array in the current
  graph and sit outside `build_log.md`/`failures.json` scope. See R-005.
- Action: ESC-002 flipped to `addressed` in place with `Verified:` line.

### R-003 — ESC-003 verified fixed, closed (T04 rework quality: clean)
- Re-verified by reviewer: `.gitignore` ends with `benchmark_results_*.tsv` +
  `corpus_info*.txt` (ESC-003 comment); `git check-ignore -v` hits both patterns;
  reviewer-ran `git status --short` shows no `?? benchmark_results_*` /
  `?? corpus_info*`. `CMakeLists.txt` + `benchmark/run_benchmark.sh` needed no
  edits (already compliant); all T04-T01..T05 re-run PASS per wave-5 log. The
  `.gitignore` edit was outside T04's declared footprint but expressly authorized
  by the wave-5 manifest + ESC-003 and explicitly flagged — accepted, not
  re-escalated. Remaining `??` entries (`.graphifyignore`, `benchmark/`,
  `current_context.txt`, `docs/`, `graphify-out.old/`, `tests/`,
  `tools/render_diff.cpp`) belong to other tasks' expected artifacts — not strays.
- Action: ESC-003 flipped to `addressed` in place with `Verified:` line.

### R-004 — New finding: task-split gap blocks ESC-001 (routes to TASK_MANAGER)
- id: R-004; linked task: T05-test-verifier; category: `gap`;
  root-cause classification: `bad_task_breakdown`; severity: `blocking`.
- Evidence: `docs/tasks/task-graph.json` `touches_files` cover T01/T02/T03/T04/T05/T06
  as listed in ESC-004; `src/core/PDFOptimizer.cpp` appears in none. T06-T04
  additionally gates `git diff src/core/PDFOptimizer.cpp` to zero lines, so even a
  hero fix would trip the acceptance gate as written. Two consecutive waves prove
  re-dispatching T05 cannot converge: the worker has no legal file to change.
- Suggested direction (non-binding): narrow product-fix task owning the optimizer
  file for the ESC-001 minimal fix only + T06 gate carve-out (see ESC-004).
- Routed: ESC-004 → TASK_MANAGER (`Status: ESCALATED_TO_TASK_MANAGER`).

### R-005 — Notes only (no escalation this pass)
- T02/T03 untouched since waves 2-3; prior review found them clean; nothing in
  waves 5-6 changes that — not re-diagnosed.
- T06-ctest-wiring-gate remains `not_started`, held back on T05 (`blocked`) — correct
  per DEPENDS_ON; will re-validate the combined tree including the future optimizer
  fix. Its T06-T04 gate wording vs the needed optimizer diff is already covered by
  ESC-004; no separate escalation.
- Pre-existing `tests/test_pdf_optimizer.cpp` failures (R-002 note) and uncommitted
  tree modifications outside task footprints (`src/main.cpp`,
  `tools/run_optimize.cpp`, `vcpkg.json`, `PDFOptimizer.h`) remain visible to the
  T06 gate; flagged here for Build Lead awareness, not reviewed (out of scope).

## Summary (this pass)
- ESC-001: still failing, independently reproduced, left open (code_bug, blocking, BUILD) + new ESC-004 (task-split, blocking, TASK_MANAGER).
- ESC-002: verified fixed, addressed. ESC-003: verified fixed, addressed.
- T01/T04 rework quality clean; no architecture, requirements, or security findings.
- No severity inflated: R-005 items are honest non-findings.

Status: REVIEW_COMPLETE

## Re-review — Wave 7 direct fixes + T06 wiring (2026-10-04)

Scope: `docs/build/build_log.md` (Wave 7 direct-fix section, `Status: NEEDS_CODE_REVIEW`),
`docs/build/failures.json` (1 blocked: T06-ctest-wiring-gate / T06-T04),
`docs/build/logs/T07-direct-fix.md` + `T08-direct-fix.md` + `T06-direct-fix.md`,
`docs/tasks/task-graph.json` (8 tasks: T01–T08 with T05→T07, T06→T07/T08 edges, amended T06-T04 carve-out),
prior entries F-001/R-001 (ESC-001), R-004 (ESC-004), and live verification below.

Graphify note: skill loaded first per protocol; `graphify-out/graph.json` does not exist
in this repo (verified via `ls graphify-out/graph.json` → No such file or directory).
No graph to query — manual inspection used (source read, `git diff`/`git status`,
reviewer-ran `generate_corpus`/`run_optimize` with `DYLD_LIBRARY_PATH=build`, full `ctest`).
Stated explicitly, not skipped.

### W7-001 — ESC-001 verified fixed, closed (T07 dedup guard quality: clean)
- Linked task: T07-optimizer-dedup-fix (`done`); originally failing test T05-T04 now green.
- Re-verified by reviewer (not taken on trust): `src/core/PDFOptimizer.cpp:372-382`
  holds the guard — `if (it->second.isIndirect()) continue;` skip + `try/catch`
  (logs + `continue`), `objGen !=` retained line 366. Reviewer-ran end to end:
  `DYLD_LIBRARY_PATH=build ./build/run_optimize transparency.pdf` prints
  `=== Optimization Successful ===` (no `indirect object handle`), `qpdf --check`
  on `transparency_optimized.pdf` clean, full `ctest` → `100% tests passed,
  0 failed out of 33` (32 pass + 1 allowed skip `BenchmarkTsvSsimThresholds`, no TSV
  present — allowed path). `build_log.md` wave 7 records T07-T01..T03 PASS, T05 `done`.
- Quality: minimal crash fix only — `git diff --stat src/codecs/` empty (no codec
  change); optimizer diff vs master is large (~286 lines) but that bulk is the
  pre-existing `OptimizationOptions`/per-item-strip refactor, not T07 (see W7-004);
  the T07-authored hunk is the dedup block comment+guard. RAII/scope discipline held
  (no raw owning pointers added; `std::cerr` log + `continue` keeps the Resilience
  constraint: one bad pair never fails the file). Observation only (not escalated):
  because `seenStreams` only stores indirect handles (line 338 filter), the
  `isIndirect` skip means dedup currently never fires (`streamsDeduplicated` stays 0)
  — safe per the ESC-001 suggested option, but a future Phase B may prefer passing a
  direct copy so dedup actually deduplicates. Honest `nice-to-have` at most; not filed.
- Action: ESC-001 flipped to `addressed` in place with `Verified:` line.

### W7-002 — ESC-004 verified fixed, closed (task-split quality: clean)
- Evidence in `task-graph.json`: new `T07-optimizer-dedup-fix`
  (`touches_files: [src/core/PDFOptimizer.cpp]`, `status: done`) and new
  `T08-test-expectations-fix` (`touches_files: [tests/test_pdf_optimizer.cpp]`,
  `status: done`); `T05.dependencies` now `["T01-corpus-hardening",
  "T07-optimizer-dedup-fix"]`; `T06.dependencies` now include `T07` + `T08`;
  T06-T04 expected_result carries the ESC-001 carve-out
  (`git diff src/core/PDFOptimizer.cpp | grep -q 'isIndirect'`, dedup-block-only,
  `git diff src/codecs/` empty). Exactly what ESC-004 asked for — no broader
  optimizer bundling, gate amended rather than waived.
- Action: ESC-004 flipped to `addressed` in place with `Verified:` line.

### W7-003 — T08 + T06-wiring quality review (done/blocked-but-correct work) — no findings
- T08 (`done`): `tests/test_pdf_optimizer.cpp:27` `stripBookmarks=true`,
  `:42-43` `stripLinks`+`stripOtherAnnotations=true`, `:57`
  `EXPECT_EQ(result.imagesSkipped, 1)` with the stale `imagesProcessed==0`
  assertion gone. This matches verified product behavior
  (`PDFOptimizer.h forProfile()`: Balanced strips JS+metadata only;
  `PDFOptimizer.cpp:235-237`: CMYK `channels==4` → `imagesSkipped++` after
  `imagesProcessed++`), so the tests now drive the option explicitly rather than
  relying on defaults — correction, not weakening. `ctest -R PDFOptimizerTest`
  4/4 green per log; reviewer full-suite run confirms. No product-code change.
  No architecture/requirements deviation (AC7 CMYK skip path exercised, GTest locked,
  pure C++). No finding.
- T06 wiring (`blocked` only on T06-T04): `CMakeLists.txt:156` adds
  `tests/test_corpus_verifier.cpp` to `pdfcompress_tests` (one-line discipline held);
  `enable_testing()` + `find_package(GTest CONFIG REQUIRED)` +
  `gtest_discover_tests` + `BUILD_TESTING ON` default all present; no Python added;
  `vcpkg.json` still `gtest`, no `catch2`. T06-T01/T06-T02/T06-T03 all PASS on
  reviewer re-run (greps + live `ctest` 100%). The worker correctly left T06-T05
  benchmark e2e unrun (would repollute corpus) with targeted verifications
  substituted — documented, not hidden. No finding against T06's own work.
- T05 (`done`): unblocked by T07 with no verifier-file change needed, as predicted
  in F-001/B5. Consistent. No finding.

### W7-004 — New finding: T06-T04 diff guard vs pre-existing foreign tree mods (routes to TASK_MANAGER)
- id: W7-004; linked task: T06-ctest-wiring-gate; failing test: T06-T04;
  category: `gap` (gate-scope / task-breakdown gap);
  root-cause classification: `bad_task_breakdown` (gate as written cannot pass on
  this tree — NOT a `code_bug` in T06's work); severity: `blocking`.
- Evidence (reviewer reproduced, not from message alone):
  `git diff --name-only` → `.gitignore`, `CMakeLists.txt`,
  `src/core/PDFOptimizer.cpp`, `src/core/PDFOptimizer.h`, `src/main.cpp`,
  `tools/run_optimize.cpp`, `vcpkg.json` (+ untracked `benchmark/`, `tests/`,
  `tools/render_diff.cpp`, `docs/` which are expected Phase-0 artifacts).
  Filtered against the amended allowed-paths
  (`tests/|benchmark/|tools/render_diff|CMakeLists.txt|docs/|src/core/PDFOptimizer.cpp`),
  foreign files remain: `.gitignore`, `src/core/PDFOptimizer.h`, `src/main.cpp`,
  `tools/run_optimize.cpp`, `vcpkg.json` → `FOREIGN-FILES-PRESENT`, so T06-T04's
  `grep -qvE` check cannot pass. `failures.json` + `T06-direct-fix.md` agree and
  the worker explicitly refused to revert/absorb them — correct call.
- Why TASK_MANAGER (not Build/Plan/Requirements): T06's code is correct; the
  architecture is sound (per-item stripping design verified in Appendix A);
  requirements need no change (AC5's intent — no Phase-0 decision-logic change —
  still holds; the tree's refactor is pre-existing in-flight work, and Appendix A
  already records per-item stripping as "FIXED on master", i.e. the gate's
  `master` baseline is stale). What must change is the task artifact: the T06-T04
  allowed-paths/baseline wording, or an explicit tree-hygiene decision
  (commit/stash/carve-out) recorded in the graph — both owned by Task Manager.
  The `.gitignore` entry is additionally a gate-wording miss: ESC-003's hygiene
  edit was expressly authorized yet `.gitignore` is still not an allowed path.
- Suggested direction (non-binding): commit the in-flight product work, stash it
  out of the Phase-0 tree, or amend T06-T04 allowed-paths/baseline — do NOT
  silently absorb unrelated in-flight work into Phase 0 to make the gate pass.
- Routed: ESC-005 → TASK_MANAGER (`Status: ESCALATED_TO_TASK_MANAGER`).

## Summary (this pass)
- ESC-001: verified fixed (independent `run_optimize` + `ctest`), addressed.
- ESC-004: verified fixed (T07/T08 + edges + gate carve-out in graph), addressed.
- T07/T08/T06-wiring quality: clean — RAII held, no weakened assertions, no
  decision-logic change. No architecture, requirements, or security findings.
- New ESC-005 (blocking, TASK_MANAGER): T06-T04 foreign-tree-mods gate gap.
- No severity inflated: the dedup-never-fires observation is an honest non-finding.

Status: REVIEW_COMPLETE
