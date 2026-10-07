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

## Verify-and-close pass — user-authorized commits (2026-10-04)

Scope: verify committed tree independently per user instruction — guard presence,
codecs untouched, gtest/no-catch2, worktree clean, reviewer re-ran `ctest`;
close-out check on ESC-005; spot-check ESC-001/002/003/004 remain addressed.

Graphify note: skill loaded first per protocol; `graphify-out/graph.json` does not
exist in this repo. No graph to query — manual verification used (git log/status/
diff, grep, reviewer-ran ctest + run_optimize + qpdf --check). Stated explicitly.

### V-001 — ESC-005 substance resolved, closed; verbatim gate now vacuous (new ESC-006)
- Verified independently (not taken on trust):
  - Commits exist, unpushed: `67a54b9` (baseline: stripping options, CLI flags, GUI
    picker, gtest dep, incl. ESC-001 guard) + `9441c8c` (Phase-0: corpus, benchmark,
    render-diff, verifier, CTest wiring, .gitignore hygiene).
  - Guard present: `src/core/PDFOptimizer.cpp:373-382` (`it->second.isIndirect()`
    skip + try/catch, `objGen !=` retained line 366).
  - `git diff 34e6301..HEAD --stat -- src/codecs/` empty — codecs untouched.
  - `CMakeLists.txt` GTest wiring + `vcpkg.json` gtest, no catch2 — confirmed.
  - Worktree `git diff --name-only` → only `docs/orchestrator_state.json`
    (orchestrator-owned, not Phase-0); untracked `??` → only personal files
    (`.graphifyignore`, `current_context.txt`, `graphify-out.old/`).
  - Reviewer re-ran `ctest --test-dir build`: `100% tests passed, 0 failed out of 33`
    (1 allowed skip `BenchmarkTsvSsimThresholds`); reviewer-ran
    `run_optimize transparency.pdf` → `=== Optimization Successful ===`,
    `qpdf --check` clean.
  - Stash-rejection evidence accepted: every alleged-foreign file is load-bearing
    (vcpkg gtest=build dep; OptimizationOptions=T05/T08 compile dep; CLI flags=
    T06-T03 dep; .gitignore=ESC-003 hygiene). Commit-over-stash was the correct call.
- Substance accepted: guard committed and scope-limited (T07-authored hunk is the
  dedup block only; the ~286-line optimizer diff vs master is the pre-existing
  refactor, not Phase-0 scope creep). ESC-005 flipped to `addressed` in place.
- Remaining Task Manager touch (filed, not fixed): T06-T04's verbatim carve-out leg
  `git diff src/core/PDFOptimizer.cpp | grep -q 'isIndirect'` is now vacuous —
  reviewer confirmed the worktree diff is empty so that grep fails despite the guard
  being present in the file. Reword to grep file content instead of diff output.
  Routed as ESC-006 → TASK_MANAGER, severity blocking (T06-T04 as written cannot
  pass verbatim), category gap. No Build/Plan/Requirements defect.
- id: V-001; linked task: T06-ctest-wiring-gate; category: `gap`;
  root-cause classification: `bad_task_breakdown`; severity: `blocking`.

### V-002 — ESC-001/002/003/004 spot-check: remain addressed (no re-diagnosis)
- ESC-001: guard present (lines 373-382) + live transparency optimize succeeds +
  full ctest green (includes `StrippingRegressionSizeGateWithExemption`). Holds.
- ESC-002: unchanged since Wave-5 close-out (photo_jpeg DCTDecode in committed
  `tests/test_corpus_generator.cpp`); nothing in the two commits regresses it. Holds.
- ESC-003: `.gitignore` hygiene committed in 9441c8c; worktree shows no
  `?? benchmark_results_*` / `?? corpus_info*`. Holds.
- ESC-004: T07/T08 + edges + gate carve-out all present in committed
  `task-graph.json` (Wave-7 verification stands; commits preserve the graph). Holds.

## Summary (this pass)
- ESC-005: verified resolved via commit-over-stash, addressed. New ESC-006
  (blocking, TASK_MANAGER): T06-T04 verbatim `git diff` grep vacuous post-commit.
- ESC-001/002/003/004: spot-checked, remain addressed. No new code bugs,
  architecture, requirements, or security findings. No severity inflated.

Status: REVIEW_COMPLETE

## Final close-out pass — formal step-12 gate (2026-10-04)

Scope: full quality review of the final tree per close-out brief —
`docs/build/build_log.md` (Waves 1–8, terminal `Status: READY_FOR_REVIEW`),
`docs/build/failures.json` (`[]`), `docs/tasks/task-graph.json` (8 tasks),
`docs/requirements/requirements.md`, `docs/architecture.md`, `docs/plan.md`,
prior entries F-001/R-001/R-004/V-001/W7-004 and ESC-001..006, live
verification below, committed scope (`git diff 34e6301..HEAD`), worktree
hygiene, security/offline, and the direct (user-authorized, agent-bypassed)
edits themselves.

Graphify note: skill loaded first per protocol; `graphify-out/graph.json`
does not exist in this repo (read attempted → File not found; prior passes
record the same). No graph to query — manual inspection used
(read/grep/shell, reviewer-ran `ctest` + `run_optimize` + `render_diff` +
`qpdf --check`). Stated explicitly per skill rules — not silently skipped.

### G-001 — All 8 task statuses justified (spot-run, not taken on trust)
- `task-graph.json`: 8/8 `"status": "done"` (T01, T02, T03, T04, T05, T06,
  T07, T08 confirmed by id-list + done-count greps). `failures.json` is `[]`.
  `build_log.md` terminal line is `Status: READY_FOR_REVIEW` (Wave 8:
  T06 reword executed literally, all legs PASS, ctest 100%).
- Reviewer re-ran `ctest --test-dir build --output-on-failure`:
  `100% tests passed, 0 failed out of 33` (1 allowed skip
  `BenchmarkTsvSsimThresholds`, no-TSV-present allowed path). Justifies every
  `done`, including T05 (StrippingRegressionSizeGateWithExemption green
  post-T07) and T06-T02.
- No failure to diagnose (zero `blocked`): entry-A path not applicable.
- No finding.

### G-002 — All 6 ESC entries properly addressed (spot-checked, not re-diagnosed)
- ESC-001 (guard): `src/core/PDFOptimizer.cpp:373` holds
  `if (it->second.isIndirect())` skip (+ try/catch, `objGen !=` retained);
  reviewer-ran `run_optimize transparency.pdf` →
  `=== Optimization Successful ===` (no `indirect object handle`),
  `qpdf --check` on `transparency_optimized.pdf` clean. Holds.
- ESC-006 (gate legs): T06-T01/T06-T04 executed literally this pass —
  LEG1 PASS (only `docs/` tracked-modified: `docs/code_review/*`,
  `docs/tasks/task-graph.json`, `docs/orchestrator_state.json` — all inside
  `docs/`, none outside), LEG2 PASS (untracked limited to
  `.graphifyignore`, `current_context.txt`, `graphify-out.old/`), LEG3 PASS
  (guard in file), LEG4 PASS (`src/codecs/` diff empty), LEG5 PASS
  (gtest present, no catch2). Holds.
- ESC-002/003/004/005: unchanged since their close-outs (DCTDecode +
  TurboJPEG in generator, `.gitignore` hygiene committed in 9441c8c,
  T07/T08 + edges in graph, foreign files committed via 67a54b9+9441c8c);
  nothing in Wave 8 regresses them — Wave 8 touched only task-graph wording.
  Prior close-outs stand.
- No finding; no status flips (all six already `addressed` with `Verified:`
  lines; this pass confirms, does not re-edit).

### G-003 — No scope creep in commits beyond reviewed footprints
- `git diff 34e6301..HEAD --name-only`: `.gitignore`, `CMakeLists.txt`,
  `benchmark/` (incl. new `generate_corpus.cpp` 9-line main — expected
  binary, within allowed `benchmark/`), `docs/*`, `src/core/PDFOptimizer.h/
  .cpp`, `src/main.cpp`, `tests/` (incl. new `test_codecs.cpp`,
  `test_decision_engine.cpp`, `test_image_analyzer.cpp` — baseline GTest
  files consistent with the locked 12-test baseline, within allowed `tests/`),
  `tools/render_diff.cpp`, `tools/run_optimize.cpp`, `vcpkg.json`. Every path
  is inside T06-AC4's allowed set; `src/codecs/*` + `DecisionEngine` diffs
  are empty (verified: 0 lines). The ~286-line optimizer diff vs master is
  the pre-existing per-item-strip refactor (committed in 67a54b9, Appendix A
  "FIXED on master"), not Phase-0 scope creep; the Phase-0-authored hunk is
  the dedup guard block only.
- Worktree `git status --short`: modified tracked = `docs/code_review/
  escalations.md`, `docs/code_review/review_log.md`,
  `docs/orchestrator_state.json`, `docs/tasks/task-graph.json` — all under
  `docs/` (this review's own output + the direct task-graph reword +
  orchestrator bookkeeping). No source/tree pollution.
- No finding.

### G-004 — No security/offline violations
- `grep -rin 'python3|scikit|pip install|ImageMagick'` over
  `tools/render_diff.cpp`, `benchmark/run_benchmark.sh`,
  `tests/test_corpus_verifier.cpp` → CLEAN (only `pipefail` substring).
  `vcpkg.json`: `gtest` once, no `catch2`. Render-diff legs verified live:
  identical PDFs → `SSIM 1.0000 PSNR INF` exit 0; encrypted →
  `N/A N/A` exit 0. Corpus remains `$TMPDIR` ephemeral; test password
  `test123` confined to fixture (38 files in TMPDIR corpus dir include
  expected `*.opt.pdf`/`*verify*` test-run byproducts — ephemeral, not
  committed, verifier counts canonical 14 robustly).
- No finding.

### G-005 — Direct edits themselves (gate reword honesty, Verified-line accuracy)
- T06 AC4/T06-T04 reword (`task-graph.json:531-534`): honest — replaces the
  vacuous post-commit `git diff | grep isIndirect` leg with tree-content
  checks (`grep -q 'isIndirect' src/core/PDFOptimizer.cpp`, codecs-empty,
  gtest/no-catch2, docs-only worktree). All legs executed literally this
  pass and PASS (G-002) — no weakening, gate pinned to the committed guard.
- ESC-006 `Verified:` line: accurate — reword present at the cited lines,
  legs verified above, ctest 100%.
- No finding.

## Summary (this pass)
- Zero new findings. No bug / quality / consistency / security / gap issue
  in the final tree beyond what prior passes already filed and closed.
- Prior close-outs (ESC-001..006) confirmed standing via independent
  re-verification — not on trust.
- No new `escalations.md` entries; no status flips this pass. The loop
  stays closed.

Status: REVIEW_COMPLETE

## Phase 1 Wave 8 review — P1-T1 done, P1-T2/P1-T3 blocked (2026-10-07)

Scope: `docs/build/build_log.md` (Wave 8 P1-T1 done / P1-T2 BLOCKED / P1-T3
BLOCKED, terminal `Status: NEEDS_CODE_REVIEW`), `docs/build/failures.json`
(P1-T2, P1-T3), `docs/build/results/_archive/wave8_P1-T*.json` (note: results
live under `_archive/`, not `docs/build/results/<id>.json`), `docs/tasks/
task-graph.json` (Phase 1 graph, P1-T1..P1-T5), `docs/requirements/
requirements.md` Part II (AC-B1/B2/B3, S1–S9, verify-only lock),
`docs/architecture.md` + `docs/plan.md` Phase 1 sections, `src/core/
PDFOptimizer.cpp:119-125` + `:334-386` (read-only diagnosis), `src/core/
PDFOptimizer.h:27-64` (forProfile), new test files (untracked), `git
status/diff`, reviewer re-ran `ctest -R 'StrippingProfiles|StructureWriter|
ProfileDefaults'`.

Incremental scope: everything since the last `Status: REVIEW_COMPLETE`
(Phase 0 final close-out). Only P1-T1/P1-T2/P1-T3 are under review; P1-T4
(not_started) and P1-T5 (not_started) are not reviewed. No prior
`docs/code_review/escalations.json` existed (legacy `escalations.md`
ESC-001..006 are all `addressed`; spot-checked ESC-001 guard still present
at `PDFOptimizer.cpp:373`, nothing regresses them — no re-diagnosis).

Graphify note: no `graphify-out/graph.json` query needed — the failures are
fully explained by the results JSON, the cited source lines, and live test
reproduction. Per instructions, `GRAPH_REPORT.md` was not read.

### P-001 — P1-T1 (done) quality review — clean, no findings

- Linked task: P1-T1 (`done`); tests P1-T1-T01..T03 all pass per results JSON
  (standalone gtest build; CMake wiring belongs to P1-T4).
- Verified by reading `tests/test_profile_defaults.cpp` (46 lines) against
  `PDFOptimizer.h:27-64`: MaxQuality all-7-false, Balanced JS+metadata-only,
  MaxCompression all-7-true, `linearize=false`, `recompressFlate=true`,
  `deduplicateStreams=true` — exact match, no product-code change (`git
  status` shows no `src/` modification; new test file is untracked, within
  `touches_files`). GTest-only, C++ hygiene held.
- Observation (not a finding): `ctest -R ProfileDefaults` finds no tests —
  expected, since `CMakeLists.txt` does not yet list the file (P1-T4 owns
  that edit; see P-004). No architecture/requirements deviation (AC-B1).

### P-002 — Failure diagnosis: P1-T2 (blocked, P1-T2-T02)

- id: P-002; linked task: P1-T2 (`blocked`); failing test P1-T2-T02
  (`StrippingProfiles.BalancedRemovesOnlyJsAndMetadata`).
- Category: `gap`; root-cause classification: `bad_task_breakdown`
  (test expectation stricter than its own task AC and incompatible with
  locked defaults — NOT a worker error in execution); severity: `blocking`
  (blocks P1-T4 via DEPENDS_ON).
- Evidence (reproduced, not from message alone): reviewer-ran `ctest
  --test-dir build -R StrippingProfiles` → same failure (`metadataStripped`
  true on with_javascript/with_form/with_bookmarks_and_links, `test_
  stripping_profiles.cpp:155,185,187,191`). Mechanism confirmed by reading
  `PDFOptimizer.cpp:119-125`: `result.metadataStripped = true` is set
  unconditionally whenever `options.stripMetadata` is true (Balanced sets it
  true), without checking whether `/Info`, `/Metadata`, `/PieceInfo`,
  `/LastModified`, or `/Thumb` was present/removed. `removeKey` on an absent
  key is a no-op, but the flag is still set — the bool reports intent while
  every sibling counter reports actual removals.
- Why TASK_MANAGER (not BUILD): the product code implements the locked
  `forProfile` behavior (Balanced `stripMetadata=true`, pinned by P1-T1);
  changing the flag to conditional would be a strip-semantics/reporting
  change barred by the Phase 1 verify-only lock. The task's own AC #2
  requires `metadataStripped=true` only on with_metadata.pdf, with the
  "nothing else" parenthetical scoped to the six removal counts — the test's
  `EXPECT_FALSE(r.metadataStripped)` on metadata-free PDFs over-asserts
  relative to its task AC. What must change is the task artifact (narrow the
  expectation), owned by Task Manager. Architecture needs no change
  (strip branches sound); the worker correctly refused both a product edit
  and a test-weakening.
- Routed: ESC-007 → TASK_MANAGER (blocking). Cross-reference: ESC-008.

### P-003 — Requirements gap behind P1-T2: metadataStripped undefined

- id: P-003; linked task: P1-T2; category: `gap`; root-cause
  classification: `requirements_gap`; severity: `should-fix`.
- Evidence: nothing defines whether `metadataStripped` means "actually
  removed something" or "strip was requested". `requirements.md` AC-B2 says
  Balanced removes "JS and metadata and nothing else" with no bool scoping;
  P1-T2 AC scopes "nothing else" to counts; architecture lists the field
  without semantics; `OptimizationResult` has no doc comment. The P1-T2
  failure is this ambiguity made concrete.
- Routed: ESC-008 → REQUIREMENTS (should-fix). If defined as
  actually-removed, the follow-on product change belongs to a future
  (non-verify-only) task — not P1-T2.

### P-004 — Failure diagnosis: P1-T3 (blocked, P1-T3-T02)

- id: P-004; linked task: P1-T3 (`blocked`); failing test P1-T3-T02
  (`StructureWriter.DedupCountedWithDefaultsZeroWithout`).
- Category: `gap`; root-cause classification: `bad_task_breakdown` (task as
  scoped cannot satisfy its AC — needs product + corpus changes outside its
  footprint); severity: `blocking` (blocks P1-T4 via DEPENDS_ON).
- Evidence (reproduced): reviewer-ran `ctest --test-dir build
  -R StructureWriter` → T01/T03 pass, T02 fails (`streamsDeduplicated==0`,
  `test_structure_writer.cpp:50`). Both worker-diagnosed causes confirmed by
  reading code: (1) `PDFOptimizer.cpp:338` stores only indirect streams in
  `seenStreams`, so the ESC-001 guard at line 373 (`if
  (it->second.isIndirect()) continue`) always fires — `replaceObject` /
  `streamsDeduplicated++` is dead code (consistent with prior review note
  W7-001, which flagged dedup-never-fires as an accepted safe limitation);
  (2) `generateSharedXObject()` returns `generateTransparency()`
  (`test_corpus_generator.cpp:450-451`), which shares a single indirect
  image object by reference across pages (`:310-316`, same handle → same
  objGen), so no distinct byte-identical stream pair exists — the `objGen !=`
  guard (line 366) would skip even under a working direct-copy replace path.
  The test file itself is correct as written (3 TESTs, GTest-only, qpdf-check
  semantics); P1-T3-T01/T03 passing confirms the harness is sound.
- Why TASK_MANAGER (not BUILD/PLAN/REQUIREMENTS directly): the worker
  correctly refused out-of-footprint edits twice over (product dedup path +
  corpus fixture). Re-dispatching P1-T3 cannot converge. The fix is a
  breakdown change: re-scope the AC (validity/size comparison only, which
  already pass) and/or add a product task (direct-copy dedup replace) plus a
  distinct-duplicate-stream fixture. Flagged inside the entry: AC-B3 promises
  `streamsDeduplicated>=1` on the shared-XObject PDF, which the
  reference-shared fixture cannot satisfy under any replace-based dedup —
  Task Manager should escalate to Requirements if AC-B3 itself needs
  rewording, but no separate REQUIREMENTS escalation is filed to avoid
  double-routing one breakdown.
- Routed: ESC-009 → TASK_MANAGER (blocking).

### P-005 — Footprint flag assessment (P1-T3 CMakeLists note) — not a problem

- The P1-T3 worker flagged that `CMakeLists.txt` registers
  `tests/test_structure_writer.cpp` (+ `tests/test_stripping_profiles.cpp`)
  although outside its `touches_files`, and states the edit predates its
  work. Verified: `git diff -- CMakeLists.txt` shows exactly those 2 added
  lines, uncommitted; P1-T3's `files_changed` lists only its test file and
  `git status` shows no `src/` modification — the worker's no-touch claim
  holds. Owner is unambiguous: P1-T4 ("Owns the only CMakeLists.txt edit in
  Phase 1") whose AC P1-T4-T01 expects exactly three added lines. The current
  2-line state is a partial pre-staging (missing
  `tests/test_profile_defaults.cpp`); P1-T4 reconciles it by adding the
  third line, no revert needed. No escalation — recorded here so P1-T4's
  diff gate is judged against the 3-line end state, not against this
  intermediate.

## Summary (this pass)

- P1-T1: verified clean (exact forProfile pin, no product change).
- P1-T2: test-vs-code judged — product code implements locked defaults;
  test over-asserts vs its task AC → ESC-007 (blocking, TASK_MANAGER) +
  ESC-008 (bool semantics, should-fix, REQUIREMENTS).
- P1-T3: test-vs-code judged — dead dedup path + reference-shared fixture
  make AC unreachable in-footprint → ESC-009 (blocking, TASK_MANAGER).
- Footprint flag: pre-existing 2-line CMakeLists staging belongs to P1-T4's
  expected end state (needs third line); not a problem, no escalation.
- Verify-only lock held: no `src/` edits in the working tree.
- No severity inflated; no open JSON entries to close out (all legacy
  ESC-001..006 remain `addressed`).

Status: REVIEW_COMPLETE

Status: REVIEW_COMPLETE

## Phase 1 final review — all 5 tasks done (2026-10-07)

Scope: `docs/build/build_log.md` (Phase 1 Wave 8–12; P1-T1..P1-T5 all `done`;
terminal `Status: READY_FOR_REVIEW`), `docs/build/failures.json` (`{}` — no
blocked tasks), `docs/build/results/_archive/wave{8,10,11,12}_P1-T*.json`
(results live under `_archive/`; no bare `docs/build/results/<id>.json`
exists), `docs/tasks/task-graph.json` (Phase 1 graph), `task_manager_notes.md`
(2026-10-07 ESC-007/ESC-009 revision), `docs/requirements/requirements.md`
Part II (AC-B1..B4, ESC-008 normative note, glossary) + `open_questions.md`,
new test files (`tests/test_profile_defaults.cpp`,
`tests/test_stripping_profiles.cpp`, `tests/test_structure_writer.cpp`),
`CMakeLists.txt` + `USAGE.md` diffs, `src/core/PDFOptimizer.{h,cpp}` (read-only
diagnosis), archived TSVs, and reviewer-ran `ctest`.

Incremental scope: everything since the last `Status: REVIEW_COMPLETE` (Phase 1
Wave 8 review). Only P1-T1..P1-T5 reviewed. Prior legacy escalations ESC-001..006
remain `addressed`, spot-consistent, not re-diagnosed.

Graphify note: `graphify-out/graph.json` now exists (created 2026-10-03), but per
the per-agent rule it was not needed — the diffs, results JSON, and direct source
reads fully answer every question here (no cross-file impact beyond
`PDFOptimizer.cpp` <-> corpus fixture, both read directly). `GRAPH_REPORT.md` was
not read. Stated explicitly, not silently skipped.

### F-0101 — ESC-007 verified resolved, closed (P1-T2 revision: genuine)
- Re-verified, not on trust: `tests/test_stripping_profiles.cpp` T02 no longer
  asserts `metadataStripped==false` on metadata-free PDFs; it asserts
  `EXPECT_TRUE(metadataStripped)` on `with_metadata.pdf` (line 162) and six
  removal counters `==0` elsewhere, matching the now-normative AC-B2 note
  (`requirements.md:350`) and glossary (`:54`). Reviewer re-ran
  `ctest -R StrippingProfiles` → 5/5 green. The old expectation simply
  contradicted the locked `forProfile` defaults once ESC-008 ruled intent
  semantics — narrowing is correct, not accommodation. → ESC-007 `addressed`.

### F-0102 — ESC-008 verified applied, closed
- Ruling is present twice in requirements (`:54` glossary, `:350` AC-B2 normative
  note) plus `open_questions.md` ESC-008 row, and the P1-T2 test matches it.
  → ESC-008 `addressed`.

### F-0103 — ESC-009 NOT resolved: revision accommodates a real gap (kept open)
- The P1-T3 task rescope (`t>1` -> pin `streamsDeduplicated==0`, both runs) does
  converge and its tests pass (reviewer `ctest -R StructureWriter` 3/3). But it
  only pins the verified *current* behavior around a genuine, still-open defect:
  `requirements.md` AC-B3 (lines 356-357) still normatively promises
  `streamsDeduplicated>=1` on the shared-XObject PDF with defaults. Reviewer
  read `PDFOptimizer.cpp:334-386`: all `seenStreams` handles are indirect, so the
  ESC-001 guard (line 373) always skips (dead `streamsDeduplicated++`), and the
  fixture reference-shares one indirect object (`objGen !=` guard line 366 would
  skip anyway). Phase 1's verify-only lock forbids the product + corpus changes
  that would make AC-B3 true. So ESC-009 stays `open` per the "revised test
  merely accommodates a real gap" rule; the remaining fix owner is Requirements,
  filed as **ESC-010** (`routed_to: REQUIREMENTS`, blocking) to reconcile AC-B3.

### F-0104 — Quality review of P1-T1..P1-T5 (all done) — clean, no findings
- P1-T1: `tests/test_profile_defaults.cpp` pins MaxQuality all-false / Balanced
  JS+metadata-only / MaxCompression all-true with `linearize=false`,
  `recompressFlate=true`, `deduplicateStreams=true` — exact match to
  `PDFOptimizer.h:27-64`. No product change.
- P1-T2: 5 tests exercise the profile removal matrix, non-JS GoTo `/OpenAction`
  survival + `/AA`-removal-when-JS-stripped (staged `$TMPDIR`-only fixture,
  never committed), and explicit-flag-wins. GTest-only, RAII (`QPDF` by value),
  no product edit.
- P1-T3: `tests/test_structure_writer.cpp` asserts default size <= no-opt, dedup
  counter `==0` (verified current behavior), and `qpdf --check` on all outputs.
  Test file correct as written.
- P1-T4: `CMakeLists.txt` diff is exactly the three added test-file lines in one
  hunk, no other change — matches its AC. Reconciled P1-T3's pre-existing 2-line
  staging (added the missing third line). Reviewer full `ctest`:
  **100% tests passed, 0 failed out of 44** (1 allowed pre-existing skip
  `CorpusGenerator.BenchmarkTsvSsimThresholds`).
- P1-T5: `USAGE.md` diff verified accurate — (a) profiles table matches
  `PDFOptimizer.h:27-64` exactly for all 3 profiles incl. `linearize off`,
  `Flate on`, `Dedup on`; (b) flag table lists `--profile/--quality/--strip-*/
  --no-strip-*/--linearize/--no-flate-recompress/--no-dedup`; the `--help` output
  is a subset (omits `--no-strip-links/annots/bookmarks/forms/dests`) but those
  flags ARE parsed (`tools/run_optimize.cpp:76-100`), so the docs are accurate
  to behavior; (c) measured numbers match the archived TSV
  (`build/benchmark_results_20261007_160128.tsv`): min `text_only.pdf -4.65%`,
  max `line_art.pdf 47.55%`, median ~11% (13 non-encrypted pdfcompress rows,
  median 11.05%), encrypted correctly discounted; (d) no stale claim remains —
  `20-80%`, slider-all-classes, linearization-always-on, empty-tests all fixed.
  Benchmark gate independently re-verified: pdfcompress rows in
  `_160003.tsv` vs `_160128.tsv` are size-identical 14/14 (only the encrypted
  row's `Time (ms)` differs 0 vs 1000) → 0.00% size delta holds.
- Observations (honest non-findings, no escalation): USAGE calls the gitignored
  `build/benchmark_results_20261007_160128.tsv` "archived" and names only that
  one path though two dual-write TSVs exist — harmless wording, task AC's
  "reported as archived" is satisfied by the TSVs + P1-T5 log naming both.

### F-0105 — Verify-only lock / no semantics change — confirmed
- `git status --short -- src/` is empty; `git diff --stat -- src/ src/codecs/` is
  empty. No product-code or behavior change anywhere in Phase 1. All tracked
  modifications are `CMakeLists.txt`, `USAGE.md`, and `docs/*`; the three new
  test files are untracked. Matches the Phase 1 no-strip-semantics-change lock.

## Summary (this pass)
- No new code bug / security / consistency finding. Phase 1 test + wiring + docs
  work is clean and the verify-only lock holds.
- ESC-007, ESC-008: independently re-verified and flipped to `addressed`.
- ESC-009: kept `open` — the rescope accommodates a real, unresolved gap
  (requirements AC-B3 still promises `streamsDeduplicated>=1`).
- New ESC-010 (`REQUIREMENTS`, blocking): reconcile AC-B3 with the verified
  `==0` behavior.
- `ctest` reconfirmed 44/44 green. No severity inflated.

Status: REVIEW_COMPLETE

## Phase 1 close-out re-verification — ESC-009/ESC-010 reconciled (2026-10-07)

Scope: re-verify and close the two escalations left open by the previous pass
(ESC-010 -> REQUIREMENTS, ESC-009 -> TASK_MANAGER), independently confirming the
Phase 1 tree is unchanged and green. Read: `docs/code_review/escalations.json`
(prior entries), `docs/requirements/requirements.md` (AC-B3 :352-391, glossary
:865-871, Assumptions Log :884, close-out note :906-911),
`docs/requirements/design_gaps.md` (row 14), `docs/requirements/open_questions.md`
(ESC-010 / ESC-010-FU rows), `docs/tasks/task_manager_notes.md` (:84-103),
`docs/tasks/task-graph.json` (P1-T3 :206-245), `tests/test_structure_writer.cpp`,
`docs/build/build_log.md` (Phase 1 Waves 8-12, all 5 done), `docs/build/failures.json`
(`{}`), `docs/build/results/_archive/wave10_P1-T3.json`.

Graphify note: no graph query needed — this pass is a targeted re-verification of two
specific documentation/artifact reconciliations plus a live test run; the diffs and
cited lines fully answer every question. `GRAPH_REPORT.md` was not read.

### C-001 — ESC-010 (REQUIREMENTS) verified resolved, closed
- Re-verified by reviewer, not on trust: `requirements.md` AC-B3 now reads
  "`streamsDeduplicated == 0` on the shared-XObject PDF with defaults **and** with
  `--no-dedup`; `qpdf --check` clean both" (:357-358) — the `>=1` promise is gone from
  the normative `Then`. The rationale is present (:359-373, both causes: ESC-001
  `isIndirect` guard at `PDFOptimizer.cpp:373` always taken -> `streamsDeduplicated++`
  dead code; `generateSharedXObject()` reference-shares one indirect object). The
  aspirational `>=1` is explicitly DEFERRED, not dropped (:375-387: owning phase
  Phase 6/Goal G, Phase 2 fallback, plus required product + corpus work), with a
  test-revisit trigger (:388-391). Carried consistently in the glossary (:865-871),
  Assumptions Log (:884), close-out note (:906-911), `design_gaps.md` row 14, and
  `open_questions.md` ESC-010 (answered).
- `ESC-010-FU` (`open_questions.md`): the deferred dedup fix's owning-phase question is
  recorded as a NON-BLOCKING requirements follow-up (recommendation: Phase 6 primary /
  Phase 2 fallback), awaiting user confirmation. Not a Phase-1 blocker — confirmed
  recorded, not treated as blocking.
- → ESC-010 flipped to `addressed` in place with a `verified:` line.

### C-002 — ESC-009 (TASK_MANAGER) verified resolved at root, closed
- Re-verified by reviewer, not on trust: `task-graph.json` P1-T3 (:206-245) is rescoped
  — AC2 and P1-T3-T02 both now assert `streamsDeduplicated==0` in BOTH runs (defaults
  and `deduplicateStreams=false`), 1:1 with `tests/test_structure_writer.cpp:53-54`
  (`EXPECT_EQ(rDefault.streamsDeduplicated, 0)` / `EXPECT_EQ(rNoDedup.streamsDeduplicated, 0)`).
  `touches_files` remains exactly `["tests/test_structure_writer.cpp"]`, which is now
  sufficient because the narrowed AC no longer requires product (`PDFOptimizer.cpp`) or
  corpus (`test_corpus_generator.cpp`) writes — the original footprint-vs-AC mismatch is
  moot (narrowed, not widened). `task_manager_notes.md` :84-103 records the
  post-reconciliation consistency check (PASS, no graph change) and the deferred `>=1`
  follow-up. Root cause (AC-B3 promising unsatisfiable behavior) is resolved by C-001.
- → ESC-009 flipped to `addressed` in place with a `verified:` line.

### C-003 — Independent re-run: 44/44 green, no product/semantics change
- Reviewer ran `ctest --test-dir build --output-on-failure`: **100% tests passed, 0
  failed out of 44** (includes `StructureWriter.DedupCountedWithDefaultsZeroWithout`,
  `StrippingProfiles.*`, `ProfileDefaults.*`). `docs/build/failures.json` is `{}`;
  `build_log.md` Phase 1 Waves 8-12 all `done`, terminal `Status: READY_FOR_REVIEW`.
- Verify-only lock holds: `git status --short -- src/` empty and
  `git diff --stat -- src/ src/codecs/` empty — no product-code or semantics change in
  Phase 1. `_archive/wave10_P1-T3.json` lists only `tests/test_structure_writer.cpp`,
  no footprint flags.

### C-004 — No new findings
- No new bug / quality / consistency / security / gap finding this pass. The two open
  escalations are resolved and closed; ESC-007/ESC-008 remain `addressed`. `ESC-010-FU`
  is an honest, non-blocking requirements follow-up and is recorded, not escalated.
- No severity inflated; no escalation filed.

## Summary (this pass)
- ESC-010: independently verified resolved (AC-B3 pins `==0` + rationale, `>=1`
  deferred, glossary/assumptions/design_gaps/open_questions consistent) -> `addressed`.
- ESC-009: independently verified resolved at root (P1-T3 rescoped to `==0`, footprint
  sufficient, `task_manager_notes.md` records it) -> `addressed`.
- `ctest` re-run 44/44 green; `git diff src/` empty (no product/semantics change).
- `ESC-010-FU` confirmed recorded as a non-blocking follow-up (not a Phase-1 blocker).
- No new findings. No open escalations remain.

Status: REVIEW_COMPLETE

## Phase 2 review — P2-T1..P2-T6 done, P2-T7 blocked (2026-10-07)

Incremental scope: everything since the last `Status: REVIEW_COMPLETE` (Phase 1
close-out re-verification). Under review: P2-T1..P2-T6 (`done`) and P2-T7
(`blocked`). Read: `docs/build/build_log.md` (Waves 14–21, terminal
`Status: NEEDS_CODE_REVIEW`), `docs/build/failures.json` (P2-T7),
`docs/build/results/_archive/wave{14,15,16,17,18,20,21}_P2-T*.json`,
`docs/build/logs/P2-T*.md`, `docs/tasks/task-graph.json` (Phase 2 graph),
`docs/requirements/requirements.md` Part II (AC-C1..C5, J-C1/J-C2, S1–S9),
`docs/architecture.md`/`docs/plan.md` Phase 2 rows, the `git diff HEAD` for every
changed src/test/benchmark file, and reviewer-run `ctest`, `run_optimize`,
`render_diff`, `qpdf --check`, plus two reviewer-built standalone probes against
`libpdfcompress_core.a` (PNG/Flate embed + alpha path). Prior escalations
ESC-007..ESC-010 remain `addressed` (spot-checked; nothing regresses them).

Graphify note: `graphify-out/graph.json` exists but was not needed — the diffs,
results JSON, and direct source reads fully answered every question (the one
cross-file interaction, DecisionEngine↔PngCodec↔PDFOptimizer, was read directly
and confirmed with a live probe). `GRAPH_REPORT.md` was not read.

### F-0201 — P2-T1: lossless FlateDecode branch embeds a PNG file (ESC-011, blocking)

- linked task: P2-T1; category: `bug`; root-cause classification: `code_bug`;
  severity: `blocking`.
- Evidence (reviewer-reproduced, not from the worker report): the `hasAlpha`
  branch (`src/core/DecisionEngine.cpp:46-50`) and the Screenshot/LineArt
  MaxQuality-no-slider branch (`:72-73`) select `m_pngCodec` and report
  `StreamFilter::FlateDecode`. `PngCodec::encode` (`src/codecs/PngCodec.cpp:61-92`)
  writes a complete PNG container (`png_write_info`/`png_write_end` → `0x89 'PNG'`
  + IHDR/IDAT/IEND), which is **not** a zlib/Flate stream. PDFOptimizer stores
  those bytes with `/Filter /FlateDecode` (`src/core/PDFOptimizer.cpp:478-490`).
  A direct `DecisionEngine::compress(..., hasAlpha=true)` probe returns
  `outFilter = FlateDecode` with bytes starting `89 50 4e 47`; a full
  `PDFOptimizer::optimize` probe on an alpha image whose PNG re-encode is smaller
  than its source Flate stream reports `imagesOptimized=1` and writes an output
  image stream with `/Filter /FlateDecode` and PNG bytes, on which
  `qpdf --check` reports `error decoding stream data for object 6 0: stream
  inflate: incorrect header check`.
- Why the tests miss it: `PDFOptimizerTest.TransparencyBaseImageStaysFlateDecode`
  only asserts the output `/Filter` is `/FlateDecode`; on `transparency.pdf` the
  source Flate stream is 680 B vs a 1052 B PNG output, so the image is
  `kept-original` (`imagesOptimized=0`) and the buggy branch never embeds. The
  P2-T1 unit test (`PngSelectedForAlpha`) checks only the filter name and that
  bytes are non-empty.
- Why `code_bug` not `requirements_gap`: AC-C2 normatively requires the lossless
  output to be "embeddable as a PDF /FlateDecode image stream"; the AC-C2 phrase
  "FlateDecode-or-PNG" does not license a PNG container (PDF has no PNG filter).
  The fix is in code (use `m_zlibCodec`, or make `PngCodec` emit zlib/IDAT).
- routed: ESC-011 → BUILD.

### F-0202 — P2-T7 T03: verifier SSIM gate over-strict (ESC-012, blocking)

- linked task: P2-T7; category: `bug`; root-cause classification: `code_bug`;
  severity: `blocking`.
- Evidence (reviewer-reproduced): `ctest --test-dir build -R
  CorpusGenerator.BenchmarkTsvSsimThresholds --output-on-failure` →
  `tests/test_corpus_verifier.cpp:525 EXPECT_GE(ssim, kMinSsim) actual 0.9735 vs
  0.98` on `large_uncompressed.pdf gs_ebook` and `gs_screen`. The test applies the
  0.98 gate to **every** numeric row of the newest TSV regardless of Tool. The
  newest TSV (`benchmark_results_20261007_173438.tsv`) has 39 files × 5 tools;
  every `pdfcompress` row is ≥ 0.98 (min 0.9831), only the two Ghostscript rows
  are below. `run_optimize`/product output is not at fault.
- Why `code_bug`: the gate is the product-quality gate (comment at
  `:501` "threshold for Balanced/MaxQuality on synthetic corpus") and must apply
  to the product's rows; applying it to third-party tools is a test-authoring
  defect. Reproducible, concrete fix (filter on Tool == `pdfcompress`, and/or
  restrict to the canonical 14).
- routed: ESC-012 → BUILD.

### F-0203 — benchmark runs the whole corpus dir, not the canonical 14 (ESC-013, should-fix)

- linked task: P2-T7; category: `bug`; root-cause classification: `code_bug`;
  severity: `should-fix`.
- Evidence: `benchmark/run_benchmark.sh:92` loops `"$CORPUS_DIR"/*.pdf`. The
  shared `$TMPDIR/pdfcompress_test_corpus` dir holds 39 non-byproduct PDFs
  (canonical 14 + non-canonical fixtures `large_uncompressed.pdf`,
  `bitpacked_bpc1.pdf`, `distinct_duplicate_streams.pdf` + many test byproducts
  such as `cmyk_image.pdf.cmyk_*.pdf`, `photo_heavy.pdf.check_*.pdf`,
  `grayscale_scan.pdf.q70.pdf` that the skip patterns at `:94` do not match).
  AC-C4 requires the comparison "on the same 14-file corpus". This is what feeds
  the over-strict verifier (F-0202) and makes the AC-C4 delta noisy.
- routed: ESC-013 → BUILD.

### F-0204 — no Phase-2 task owns the verifier/benchmark fix (ESC-014, blocking, TASK_MANAGER)

- linked task: P2-T7; category: `gap`; root-cause classification:
  `bad_task_breakdown`; severity: `blocking`.
- Evidence: P2-T7 `touches_files` is `["USAGE.md"]`, but its T03 gate requires a
  green `ctest`; the failing file (`tests/test_corpus_verifier.cpp`) and the
  contaminating loop (`benchmark/run_benchmark.sh`) are in **no** Phase-2 task's
  `touches_files` (P2-T1..P2-T7). Re-dispatching P2-T7 cannot converge — same
  pattern as Phase-0 ESC-004/ESC-005. The code fixes (F-0202/F-0203) need an owner
  task, or the T03 gate needs an explicit carve-out for the Phase-0 verifier
  artifact.
- routed: ESC-014 → TASK_MANAGER.

### F-0205 — dedup signature omits alpha keys (ESC-015, should-fix)

- linked task: P2-T6; category: `quality`; root-cause classification: `code_bug`;
  severity: `should-fix`.
- Evidence: the dedup signature (`src/core/PDFOptimizer.cpp:527-535`) keys on
  `/Subtype /Width /Height /ColorSpace /BitsPerComponent /Filter /DecodeParms`
  + raw-bytes hash but omits `/SMask`, `/Mask`, `/Type`, `/Intent`. Two
  byte-identical image streams differing only by carrying a mask collide and are
  merged by `ReferenceRewriter::repointAll`; the non-alpha image's referrers then
  point at the alpha object (or vice versa), silently changing transparency.
  Trigger is profile-independent when both source streams are kept original, and
  reachable under MaxQuality-no-slider when both are re-encoded identically.
  AC-C2 requires alpha never be mishandled; the AC-C5 key list does not include
  the alpha keys. The `objGen !=` guard and per-pair try/catch are correctly kept,
  and the P2-T6 test (1 vs 2 image streams, `>=1`/`==0`) is sound.
- routed: ESC-015 → BUILD.

### Quality review of the done tasks (P2-T1..P2-T6)

- **P2-T1** — routing rework is otherwise correct: slider only affects
  Photo/Screenshot/LineArt; ScannedText→JPX and Monochrome→Flate ignore it;
  defaulted `compress()` params keep existing callers compiling; 5/5 DecisionEngine
  tests green. Only F-0201 (PNG/Flate) mars it.
- **P2-T2** — `hasAlpha = hasKey("/SMask")||hasKey("/Mask")`; mask streams are
  never enumerated by `page.getImages()` so `/SMask` is left intact (verified:
  output SMask stream is valid zlib, geometry 200×150). `render_diff
  transparency.pdf` → `SSIM 1.0000 PSNR INF`. But the integration test passes on
  the `kept-original` path (see F-0201), so it does not exercise the new branch.
- **P2-T3** — DeviceGray→ScannedText and `/BitsPerComponent != 8` skip-with-reason
  are correct; the worker's fixture fix (zlib-compressing the packed bytes before
  `replaceStreamData(..., /FlateDecode, ...)`) is a genuine correction, not a
  weakening — the test now asserts the stream is byte-preserved and valid. All
  15 ImageAnalyzer|PDFOptimizer tests green.
- **P2-T4** — CMYK preserve path logs a reason, counts `cmykPreserved` and
  `imagesSkipped`, leaves the stream/ColorSpace untouched; ICCBased `/N==4`
  detected as CMYK. `CmykSkipped` retained. Correct.
- **P2-T5** — transcode is opt-in, default preserves; `--transcode-cmyk-to-rgb`
  in `--help` and USAGE; GUI checkbox wired; `JpegCodec::encodeCmykAsRgb` goes
  through `CmykHandler` with Adobe APP14/Decode inversion. Reviewer ran the
  manual gates the log omitted: `run_optimize cmyk_image.pdf
  --transcode-cmyk-to-rgb` → `CMYK Transcoded: 1`, and
  `render_diff cmyk_image.pdf <transcoded>` → `SSIM 0.9992 PSNR 41.3` (≥0.98).
  Note: the internal CIE76 delta-E gate compares the transcode against the same
  transform, so it validates channel-order/stride but **cannot** detect a wrong
  inversion decision (self-consistent by construction) — the external SSIM gate
  is what covers that; documented in the header, acceptable.
- **P2-T6** — reference-rewriting dedup works (`distinct_duplicate_streams.pdf`
  31.19% smaller, `Streams Deduplicated: 1`, SSIM 1.0); the P1-T3 test rewrite to
  `>=1`/`==0` is exactly AC-C5's requirement (not a weakening); `ReferenceRewriter`
  traversal never follows indirect refs so cycles terminate and cost is bounded
  by object count — verified correct. Only F-0205 (alpha keys) noted.

### Footprint flags assessed

- **P2-T5 `src/codecs/JpegCodec.h`** (not in `touches_files`): assessed as a
  legitimate, unavoidable companion declaration for `JpegCodec.cpp`'s new
  `encodeCmykAsRgb` (declared `.h`, defined `.cpp`, called from
  `PDFOptimizer.cpp`). The edit is correct and no downstream gate depends on it;
  recorded as a task-graph completeness observation (Task Manager may add the
  header to `touches_files` in future graphs) — **not escalated**.
- **P2-T3 shared-checkout note** ("working tree also contains other tasks'
  changes"): verified benign — the listed files belong to P2-T1/P2-T5; the P2-T3
  worker touched only its own four files. No action.

### Observations (honest non-findings, no escalation)

- `streamsDeduplicated` counts *references repointed*, not streams collapsed
  (`PDFOptimizer.cpp:561-563`). For the AC (`>=1`) this is fine; if a duplicate
  has multiple referrers the number over-reports streams. Naming nit only.
- P2-T3's DeviceGray rule classifies any single-channel image with >4 sampled
  colours as ScannedText, so a natural grayscale photo now routes to JP2 rather
  than Photo/DCT. Consistent with the AC controls; no requirement violated.
- P2-T7-T04's evidence gap: the archived TSV is a **default-profile** run, so its
  "screenshot/line-art MaxQuality rows change codec" leg is not actually
  demonstrated by the TSV (those rows are Balanced/DCTDecode). P2-T7 is blocked
  on T03 anyway; T04 evidence should be revisited after ESC-012/013.
- P2-T5's manual T03/T04 were not recorded by the worker; reviewer verified both
  pass (above), so the AC holds — evidence-recording gap only.
- `hasAdobeApp14` scans the whole JPEG for `FF EE 'Adob'`; a false positive in
  entropy-coded data is possible but only affects the opt-in path, which the
  external SSIM gate backstops. Non-finding.

### Prior escalations

- ESC-007/ESC-008 (`addressed`): `StrippingProfiles` 5/5 green on reviewer
  re-run; requirements glossary/AC-B2 note intact — hold, no flip.
- ESC-009/ESC-010 (`addressed`): the Phase-1 deferred `streamsDeduplicated>=1`
  follow-up is now implemented by P2-T6 (AC-C5) and verified live — hold (the
  deferral is satisfied), no flip. New dedup concern filed separately as ESC-015.
- No `open` prior entries required close-out this pass.

## Summary (this pass)

- P2-T1: **ESC-011 (blocking, BUILD)** — lossless branch embeds a PNG container
  under `/FlateDecode`; silent image corruption when the re-encode wins. Tests
  miss it because the fixture takes the kept-original path.
- P2-T7: **ESC-012 (blocking, BUILD)** verifier gates all tools' rows;
  **ESC-013 (should-fix, BUILD)** benchmark loops the whole corpus dir;
  **ESC-014 (blocking, TASK_MANAGER)** no Phase-2 task owns the fix.
- P2-T6: **ESC-015 (should-fix, BUILD)** dedup signature omits `/SMask`/`/Mask`.
- P2-T2/T3/T4/T5 and the rest of P2-T1/P2-T6: verified clean (live tests +
  probes); no weakened tests; verify-only concerns N/A (Phase 2 is a code phase).
- Footprint flags assessed; P2-T5 `JpegCodec.h` not escalated.
- No severity inflated: the `streamsDeduplicated` naming, DeviceGray broadening,
  T04 evidence gap, and APP14 scan are honest non-findings.

Status: REVIEW_COMPLETE

## Phase 2 final close-out — P2-T8/T9/T10 landed, ESC-011..015 (2026-10-07)

Incremental scope: everything since the last `Status: REVIEW_COMPLETE` (Phase 2
review). Under review: the five rework tasks that address my open escalations —
P2-T8 (`done`, wave23), P2-T9 (`done`, wave24), P2-T10 (`done`, wave24), plus the
P2-T7 re-run (`done`, wave25). Read: `docs/build/build_log.md` (Waves 14–25,
terminal `Status: READY_FOR_REVIEW`), `docs/build/failures.json` (`{}`),
`docs/build/results/_archive/wave{23,24,25}_P2-T*.json`, `docs/tasks/task-graph.json`
(P2-T7..P2-T10), `docs/requirements/requirements.md` (AC-C1..C5), and the live
diffs for `src/codecs/PngCodec.cpp`, `src/core/PDFOptimizer.cpp` (dedup signature +
loop), `benchmark/run_benchmark.sh`, `tests/test_corpus_verifier.cpp`,
`tests/test_codecs.cpp`, `tests/test_pdf_optimizer.cpp`,
`tests/test_structure_writer.cpp`, `tests/test_corpus_generator.{h,cpp}`, `USAGE.md`.
Reviewer re-ran `ctest` (62/62) and inspected the newest benchmark TSV.

Graphify note: `graphify-out/graph.json` exists but was not needed — the diffs,
results JSON, and direct source reads fully answered every question (the one
cross-file interaction, DecisionEngine↔PngCodec↔PDFOptimizer, was read directly).
`GRAPH_REPORT.md` was not read. Stated explicitly, not silently skipped.

### Z-001 — ESC-011 (PngCodec under FlateDecode) verified fixed, closed
- Re-verified, not on trust: `src/codecs/PngCodec.cpp::encode` now emits a plain
  zlib stream via `compress2()` (`<zlib.h>`, no `png.h`/`png_*` anywhere in `src/`,
  no libpng in `CMakeLists.txt`). Reviewer ran `ctest -R 'CodecTest.
  PngEncodeIsFlateDecodable|PDFOptimizerTest.AlphaLosslessReencodeIsEmbeddableFlate|
  DecisionEngine.PngSelectedForAlpha|DecisionEngine.MaxQualityLosslessUsesPng'`
  → 4/4 green. `CodecTest.PngEncodeIsFlateDecodable` `uncompress()`es the codec
  output and asserts round-trip + no `0x89 'PNG'` signature; `PDFOptimizerTest.
  AlphaLosslessReencodeIsEmbeddableFlate` forces `imagesOptimized==1` on an
  `/SMask` RGB image (source stream stored at zlib level 0) and asserts the
  embedded `/FlateDecode` stream inflates to the original pixels and loads clean.
  `m_pngCodec` stays reachable. → ESC-011 `addressed`.

### Z-002 — ESC-012 + ESC-013 (verifier gate + benchmark canonical-14) verified fixed, closed
- Re-verified, not on trust: `tests/test_corpus_verifier.cpp:501-545` resolves the
  `Tool` column and applies `EXPECT_GE(ssim, kMinSsim)` only when
  `toolCell == "pdfcompress"` (header/PSNR/N-A handling preserved); reviewer ran
  `CorpusGenerator.BenchmarkTsvSsimThresholds` → Passed. `benchmark/run_benchmark.sh`
  now loops an explicit `CANONICAL_FILES` array of the 14 basenames instead of
  `"$CORPUS_DIR"/*.pdf`. Reviewer inspected the newest TSV
  (`build/benchmark_results_20261007_180543.tsv`): exactly 14 canonical files ×
  5 tools, every pdfcompress row SSIM 1.0000 (encrypted N/A), no non-canonical
  fixture and no test byproduct present, and no row of any tool below 0.98.
  → ESC-012 and ESC-013 `addressed`.

### Z-003 — ESC-014 (task split) verified fixed, closed
- Re-verified, not on trust: `task-graph.json` now holds `P2-T8`
  (`touches_files: [tests/test_corpus_verifier.cpp, benchmark/run_benchmark.sh]`),
  `P2-T9` (`[src/core/DecisionEngine.cpp, src/codecs/PngCodec.cpp,
  src/codecs/CodecInterface.h, tests/test_codecs.cpp, tests/test_pdf_optimizer.cpp]`)
  and `P2-T10` (`[src/core/PDFOptimizer.cpp, tests/test_structure_writer.cpp,
  tests/test_corpus_generator.h, tests/test_corpus_generator.cpp]`); all `done`,
  all `dependencies: []`. These own exactly the ESC-011/012/013/015 fix files and
  none widen P2-T7. P2-T7's `dependencies` now include P2-T8/T9/T10 and it re-ran
  `done` (wave25, 4/4). The Phase-2 ctest gate can now pass and does.
  → ESC-014 `addressed`.

### Z-004 — ESC-015 (dedup signature /SMask /Mask) verified fixed, closed
- Re-verified, not on trust: `src/core/PDFOptimizer.cpp:534-545` appends `/Filter`,
  `/DecodeParms`, `/SMask`, `/Mask`, `/Type`, `/Intent` to the dedup signature when
  present. New fixture `generateMaskedDuplicateStreams()`
  (`tests/test_corpus_generator.cpp`) builds two byte-identical
  `/BitsPerComponent 1` image streams where only one carries an `/SMask`; new test
  `StructureWriter.MaskedStreamsNotDeduplicated` asserts `streamsDeduplicated==0`,
  three distinct image streams remain, and the single byte-identical pair differs
  on `/SMask` (proving the key — not re-encoding — kept them apart). Reviewer ran
  it → Passed. → ESC-015 `addressed`.

### Z-005 — Full suite + no weakened tests / no out-of-scope product change
- Reviewer ran `ctest --test-dir build --output-on-failure`:
  **100% tests passed, 0 failed out of 62** (no skips). This includes
  `PngEncodeIsFlateDecodable`, `AlphaLosslessReencodeIsEmbeddableFlate`,
  `MaskedStreamsNotDeduplicated`, `DedupCountedWithDefaultsZeroWithout`,
  `BenchmarkTsvSsimThresholds`, and both `DecisionEngine` PNG cases.
- No weakened tests: the ESC-015 fixture is genuinely adversarial (only the
  `/SMask` key differs; without the fix it would merge — the test's
  `identicalPairs==1` + `EXPECT_NE(hasMask)` pair check proves the key is doing
  the work), the P1-T3 dedup test was *strengthened* to `>=1`/`==0` plus stream
  counting, and the verifier change only scopes the existing gate, it does not
  lower `kMinSsim` or drop assertions. The P2-T8/T9/T10 `files_changed` sets match
  their `touches_files` (no footprint flags in any results JSON).
- No out-of-scope product change: the only source change in the incremental scope
  is `src/codecs/PngCodec.cpp` (P2-T9) and `src/core/PDFOptimizer.cpp` (P2-T10,
  dedup signature only); all other modified `src/` files belong to the already
  reviewed P2-T1..P2-T6.

### Z-006 — New finding: stale libpng claims after the PngCodec→zlib switch (ESC-016)
- id: Z-006; linked task: P2-T9; category: `consistency`; root-cause
  classification: `code_bug` (documentation/manifest accuracy, no behaviour
  impact); severity: `nice-to-have`.
- Evidence: PngCodec no longer uses libpng, yet `src/codecs/PngCodec.h:7`
  documents "PNG encoder using libpng (lossless, supports alpha)", `name()` at
  `:10` returns `"libpng"`, `USAGE.md:16/:60/:305` still say "PNG (via libpng)"
  and list libpng among linked libraries, and `vcpkg.json` still declares the
  unused `libpng` dependency. Reviewer confirmed via repo-wide grep that no
  `png.h`/`png_*` symbol remains and `CMakeLists.txt` has no libpng reference.
- Suggested direction: make the descriptive text match the zlib implementation
  (keeping the `PngCodec` class name per AC-C2), and drop libpng from
  `vcpkg.json` if nothing links it. Honest `nice-to-have` — no test, behaviour,
  security or architecture impact.
- Routed: ESC-016 → BUILD.

## Summary (this pass)
- ESC-011/012/013/014/015: independently re-verified fixed and flipped to
  `addressed` in place (live tests + source diffs + TSV inspection, never on the
  owning agent's say-so).
- New ESC-016 (`BUILD`, `nice-to-have`): stale libpng naming/comment/manifest
  after the PngCodec→zlib switch. Non-blocking.
- `ctest` 62/62 green; no weakened tests; no out-of-scope product change.
- No severity inflated. No PLAN/REQUIREMENTS escalation open (ESC-008/ESC-010
  remain `addressed`). One non-blocking BUILD escalation (ESC-016) remains open.

Status: REVIEW_COMPLETE

## Phase 2 close-out — P2-T11 (ESC-016) + residual CodecInterface.h example (2026-10-07)

Incremental scope: everything since the last `Status: REVIEW_COMPLETE` (Phase 2
final close-out, line 1188). The only build result newer than `review_log.md`
(18:12) is `docs/build/results/_archive/wave27_P2-T11.json` (18:31); no other
result post-dates the boundary, so P2-T11 is the sole task under review. Read:
`docs/build/build_log.md` (Wave 27, terminal `Status: READY_FOR_REVIEW`),
`docs/build/failures.json` (`{}`), `docs/build/results/_archive/wave27_P2-T11.json`,
`docs/build/logs/P2-T11.md`, `docs/build/wave/P2-T11.md` (task + context pack),
`docs/tasks/task-graph.json` (P2-T11), and the live diffs for
`src/codecs/PngCodec.h` and `USAGE.md`.

Graphify note: `graphify-out/graph.json` exists but was not needed — the P2-T11
change is a two-file, two-line labelling edit and direct reads of the diff,
`PngCodec.h`, `USAGE.md`, `vcpkg.json`, `CMakeLists.txt` and the four `*.name()`
definitions fully answered every question. `GRAPH_REPORT.md` was not read. Stated
explicitly, not silently skipped.

### Z-007 — P2-T11 verified: PngCodec.h + USAGE.md libpng labelling dropped; 62/62 green
- Re-verified, not on trust. `src/codecs/PngCodec.h` grep for `libpng` returns
  nothing: `:7` reads `/// Lossless zlib/Flate encoder (supports alpha).` and
  `name()` at `:10` returns `"flate"`; the class name `PngCodec` is retained per
  AC-C2 and `m_pngCodec` stays reachable (`DecisionEngine.cpp:13` construct,
  `:49`/`:72` select). `USAGE.md` grep for `libpng` returns nothing: `:16`
  `**PNG** (via zlib/Flate)`, `:60` linked-library list now `(qpdf,
  libjpeg-turbo, openjpeg, libdeflate, zlib)`, `:305`
  `PngCodec.h/.cpp  zlib/Flate encoder (lossless, alpha support)`.
- Reviewer independently re-ran `cmake -B build
  -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake`
  (clean), `cmake --build build` (clean, all targets built) and
  `ctest --test-dir build --output-on-failure` → **100% tests passed, 0 failed
  out of 62** (no skips). Repo-wide grep confirms no `png.h`/`png_*` symbol in
  `src/`, `tests/` or `tools/`, and no test asserts `.name()`, so the `"libpng"`
  → `"flate"` change breaks nothing.
- Footprint: `files_changed` in the results JSON is exactly
  `[src/codecs/PngCodec.h, USAGE.md]` = `touches_files`; `footprint_flags: []`.
  No test file, no product-behaviour file, no coordination file touched. Not a
  weakened test, not an out-of-scope product change.

### Z-008 — ESC-016 verified fixed, closed (labelling) + libpng retention accepted as justified
- ESC-016's actionable scope (make the descriptive text match the zlib
  implementation) is fully done and independently verified in Z-007.
- The `vcpkg.json`/`CMakeLists.txt` retention flagged in ESC-016's original
  "expected" is now an ACCEPTED, documented decision, **not a defect**:
  `vcpkg.json:6` still declares `libpng` and `CMakeLists.txt:16
  find_package(PNG REQUIRED)` / `:76 PNG::PNG` still link it. This is correct
  because `requirements.md:29` lists libpng inside the hard-constraint tech
  stack ("Treat the existing tech stack above as a hard constraint — do not
  suggest replacing it") and `architecture.md` names it in 6 rows (:54, :69,
  :117, :138, :225, :234); dropping a genuinely-unused dependency is therefore
  not cosmetic — it requires a flagged requirements change plus a Validator
  architecture refresh, explicitly carved out of ESC-016's cosmetic scope
  (task_manager_notes.md, P2-T11 narrowing). The code/docs no longer *claim* a
  libpng dependency, which was the actual defect.
- → ESC-016 `status: "addressed"` with a `verified:` line recording the
  retained-dependency rationale. Re-verified on the artifacts themselves, not on
  the owning agent's say-so.

### Z-009 — New finding: stale `CodecInterface.h` codec-name example (ESC-017)
- id: Z-009; linked task: P2-T11; category: `consistency`; root-cause
  classification: `code_bug` (developer-facing documentation accuracy, no
  behaviour impact); severity: `nice-to-have`.
- Evidence: `src/codecs/CodecInterface.h:38` documents
  `Returns the name of the codec (e.g., "TurboJPEG", "libpng", "OpenJPEG").`
  After P2-T11 no codec returns `"libpng"`: `JpegCodec.h:10` → `"TurboJPEG"`,
  `Jp2Codec.h:10` → `"OpenJPEG"`, `ZlibCodec.h:9` → `"zlib"`,
  `PngCodec.h:10` → `"flate"`. The example lists a value no implementation
  produces — the same class of stale labelling P2-T11 removed from
  `PngCodec.h`/`USAGE.md`, missed in this developer-facing header (it was
  outside P2-T11's footprint).
- Suggested direction: replace the stale `"libpng"` example token with
  `"flate"` (or drop it). Docs-only; no behaviour/test/architecture impact; do
  not rename the `PngCodec` class per AC-C2.
- Routed: ESC-017 → BUILD. Honest `nice-to-have`, non-blocking — it does not
  reopen ESC-016 and does not gate the Phase-2 close-out.
- Deliberately NOT escalated (recorded so it is not silently lost): the stale
  libpng *behaviour* rows in Validator-owned `docs/architecture.md` (:54, :69,
  :117, :138, :225, :234) and the historical/accurate comment at
  `src/codecs/PngCodec.cpp:14`. The architecture rows are part of the same
  deferred stack-change cascade explicitly accepted in Z-008 (requirements
  change + Validator refresh), and the `PngCodec.cpp:14` comment accurately
  describes the ESC-011 history it documents — neither is a defect.

### Z-010 — ESC close-out sweep
- Every prior escalation in `escalations.json` is `addressed` (ESC-007..ESC-016);
  the only non-`addressed` entry after this pass is the new ESC-017 (BUILD,
  `nice-to-have`, open). No PLAN/REQUIREMENTS/TASK_MANAGER escalation is open.
- Tests: reporter re-ran the full suite → 62/62 green. No weakened tests (P2-T11
  touched no test). No out-of-scope product change (only `PngCodec.h` comment +
  `name()` label and `USAGE.md` prose changed).
- No severity inflated: ESC-017 is honestly `nice-to-have`; the architecture-doc
  staleness is an accepted deferral, not a fresh finding.

## Summary (this pass)
- P2-T11 independently verified: libpng labelling gone from `PngCodec.h` and
  `USAGE.md`, class/`m_pngCodec` intact, libpng retained in `vcpkg.json` +
  `CMakeLists.txt`; configure + build + full `ctest` 62/62 green.
- ESC-016 flipped to `addressed` in place, with the retained-dependency
  rationale recorded and verified.
- One new non-blocking BUILD escalation opened: ESC-017 (`nice-to-have`,
  `CodecInterface.h:38` stale codec-name example).

Status: REVIEW_COMPLETE

## Phase 2 close-out — P2-T12 lands ESC-017; all escalations addressed (2026-10-07)

Incremental scope: everything since the last `Status: REVIEW_COMPLETE` (Phase 2
close-out — P2-T11, line 1292). The only build result newer than that boundary is
`docs/build/results/_archive/wave29_P2-T12.json` (wave29); no other result
post-dates it, so P2-T12 is the sole task under review. Read:
`docs/build/build_log.md` (Wave 29, terminal `Status: READY_FOR_REVIEW`),
`docs/build/failures.json` (`{}`), `docs/build/results/_archive/wave29_P2-T12.json`,
`docs/build/logs/P2-T12.md`, `docs/build/wave/P2-T12.md` (task + context pack),
`docs/tasks/task-graph.json` (P2-T12), the live `git diff -- src/codecs/
CodecInterface.h`, and the four `*.name()` definitions.

Graphify note: `graphify-out/graph.json` exists but was not needed — the P2-T12
change is a one-line doc-comment edit and a direct read of the diff, the header,
the four `name()` returns and a full-suite run fully answered every question.
`GRAPH_REPORT.md` was not read. Stated explicitly, not silently skipped.

### Z-011 — P2-T12 verified: stale `libpng` example gone from CodecInterface.h:38; 62/62 green
- Re-verified, not on trust. `src/codecs/CodecInterface.h:38` now reads
  `/// Returns the name of the codec (e.g., "TurboJPEG", "OpenJPEG", "zlib", "flate").`
  — the stale `"libpng"` token is removed and every example token is a string an
  actual codec returns. Cross-checked live: `JpegCodec.h:10` `"TurboJPEG"`,
  `Jp2Codec.h:10` `"OpenJPEG"`, `ZlibCodec.h:9` `"zlib"`, `PngCodec.h:10` `"flate"`.
  Reviewer ran `grep -rn 'libpng' src/ tests/ tools/` → only
  `src/codecs/PngCodec.cpp:14` remains (a historical comment accurately describing
  the ESC-011 libpng→zlib swap, accepted in Z-009 as non-defect); no live claim remains.
- No behaviour/interface change: `git diff -- src/codecs/CodecInterface.h` shows the
  P2-T12-authored hunk is the `name()` doc-comment line only; the `virtual std::string
  name() const = 0;` declaration is unchanged. The co-located `CompressionParams`
  lossless-hint hunk in the same diff is pre-existing from P2-T9/P2-T1 (the worker
  flagged it in the log; not authored here), so it is not a P2-T12 footprint issue.
- Independent re-run: `ctest --test-dir build --output-on-failure` →
  **100% tests passed, 0 failed out of 62** (no skips), including
  `CodecTest.PngEncodeIsFlateDecodable`. Footprint: `files_changed` is exactly
  `[src/codecs/CodecInterface.h]` = `touches_files`; `footprint_flags: []`.

### Z-012 — ESC-017 verified resolved and closed; every other escalation remains addressed
- ESC-017 re-verified on the artifact itself (Z-011) and flipped to `addressed` in
  place with a `verified:` line. The example now lists only real codec-name strings,
  exactly as the entry's `expected` requires.
- Close-out sweep: `docs/code_review/escalations.json` holds 11 entries (ESC-007..017),
  **all `addressed`, none `open`** (confirmed by re-parsing the JSON). ESC-016 stays
  `addressed` (its actionable labelling scope was completed by P2-T11; the retained
  libpng build dependency remains the accepted, documented decision recorded in Z-008,
  not a defect). ESC-007..015 unchanged since their close-outs — nothing in P2-T12
  (a single doc comment) can regress them; no re-diagnosis.

### Z-013 — No new findings (clean close-out)
- No new bug / quality / consistency / security / gap finding this pass. P2-T12 is a
  documentation-only edit that is accurate, in-footprint and non-regressing.
- No severity inflated: the only `libpng` residue in `src/` is the accepted historical
  comment at `PngCodec.cpp:14`, which is not a finding.
- Honest non-finding: the `P2-T12` task's `architecture_excerpt` still describes the
  codec layer as "PNG via libpng" (quoting Validator-owned `docs/architecture.md`);
  this is the same deferred architecture-doc staleness already accepted in Z-008/Z-009
  (a requirements-change + Validator-refresh cascade, not a Build defect) and is out of
  Code Reviewer's fix ownership — recorded, not escalated.

## Summary (this pass)
- P2-T12 independently verified: `CodecInterface.h:38` no longer cites `libpng`, the
  example lists only real codec-name strings (`TurboJPEG`/`OpenJPEG`/`zlib`/`flate`),
  no interface/behaviour change, full `ctest` 62/62 green.
- ESC-017 flipped to `addressed` in place, verified on the artifact.
- All 11 escalations (ESC-007..017) confirmed `addressed`; **no open escalations remain**.
- No new findings; no severity inflated. Phase 2 close-out is clean.

Status: REVIEW_COMPLETE
