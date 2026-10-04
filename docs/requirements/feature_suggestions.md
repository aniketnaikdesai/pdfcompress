# Feature Suggestions — Phase 0 (not in scope until approved)

| # | Suggestion | Gap / opportunity | Why not in original ask | Suggested priority |
|---|---|---|---|---|
| 1 | Add `tests/test_render_diff.cpp` that renders page 1 at 150 DPI purely in C++ via PDFium and asserts SSIM ≥ threshold (no Python) | Benchmark render-diff is now spec'd as pure C++ PDFium (2026-10-03 decision: no Python scikit-image); CTest gate would make quality guard testable in CI | User asked for benchmark script only; C++ render-diff gate is optional but aligns with pure-C++ constraint | med |
| 2 | Add `--corpus-dir` flag to generate_corpus and run_optimize for deterministic output location | Current `$TMPDIR/pdfcompress_test_corpus` is ephemeral; debugging benchmark failures is harder but user confirmed TMPDIR is desired | Not in Phase 0 brief but helps reproducibility; keep TMPDIR default | low |
| 3 | Extend `diagnose` to print OptimizationResult-style breakdown for before/after comparison | diagnose only lists images, not stripping counts | Would make manual verification easier | low |
| 4 | Add GUI password dialog for encrypted PDFs (prompt for `test123` in tests, user-supplied in production; pass to QPDF/PDFium, write unencrypted unless re-encrypt opted) | AC3 encrypted fixture uses test123 for testing; production must prompt (2026-10-03) — beyond Phase 0 but noted | Phase H work, not Phase 0 | low |

