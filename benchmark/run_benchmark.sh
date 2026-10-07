#!/bin/bash
set -euo pipefail

BUILD_DIR="${1:-build}"
if ! [ -d "$BUILD_DIR" ]; then
    echo "Build directory $BUILD_DIR not found. Run cmake --build first."
    exit 1
fi

echo "Generating corpus..."
(cd "$BUILD_DIR" && ./generate_corpus) | tee corpus_info.txt
CORPUS_DIR=$(grep -F 'Corpus generated at:' corpus_info.txt | awk -F'"' '{print $2}')
if [ -z "$CORPUS_DIR" ]; then
    CORPUS_DIR=$(grep -F 'Corpus generated at:' "$BUILD_DIR"/corpus_info.txt 2>/dev/null | awk -F'"' '{print $2}' || true)
fi
if [ -z "$CORPUS_DIR" ]; then
    CORPUS_DIR="$BUILD_DIR/corpus"
fi

RESULTS_FILE="benchmark_results_$(date +%Y%m%d_%H%M%S).tsv"

echo -e "File\tOriginal Size\tTool\tOutput Size\tReduction %\tTime (ms)\tSSIM\tPSNR\tStatus" > "$RESULTS_FILE"
if [ ! -x "$BUILD_DIR/render_diff" ]; then
    echo "# render-diff unavailable (PDFium C++ only) - SSIM/PSNR columns fall back to N/A" >> "$RESULTS_FILE"
fi

run_tool() {
    local file=$1
    local orig_size=$2
    local tool_name=$3
    local cmd=$4
    local out_file=$5

    local start_ms
    local end_ms
    local elapsed

    if command -v gdate >/dev/null 2>&1; then
        start_ms=$(gdate +%s%3N)
    else
        start_ms=$(date +%s%3N 2>/dev/null || true)
        if ! echo "$start_ms" | grep -qE '^[0-9]+$'; then
            start_ms=$(( $(date +%s) * 1000 ))
        fi
    fi

    eval "$cmd" > /dev/null 2>&1 || true

    if command -v gdate >/dev/null 2>&1; then
        end_ms=$(gdate +%s%3N)
    else
        end_ms=$(date +%s%3N 2>/dev/null || true)
        if ! echo "$end_ms" | grep -qE '^[0-9]+$'; then
            end_ms=$(( $(date +%s) * 1000 ))
        fi
    fi
    elapsed=$((end_ms - start_ms))
    if [ "$elapsed" -lt 0 ]; then elapsed=0; fi

    if [ -f "$out_file" ]; then
        local out_size
        out_size=$(stat -f%z "$out_file" 2>/dev/null || stat -c%s "$out_file")
        local red
        if command -v awk >/dev/null 2>&1; then
            red=$(awk "BEGIN {printf \"%.2f\", (1-$out_size/$orig_size)*100}")
        else
            red=$(echo "scale=2; (1 - $out_size / $orig_size) * 100" | bc)
        fi
        local SSIM="N/A"
        local PSNR="N/A"
        if [ -x "$BUILD_DIR/render_diff" ]; then
            local RD_OUT
            RD_OUT=$("$BUILD_DIR/render_diff" "$file" "$out_file" 2>/dev/null || echo "N/A N/A")
            local RD_VALS
            RD_VALS=$(echo "$RD_OUT" | grep -oE '[0-9]+\.[0-9]+|INF|N/A' || true)
            local _ssim _psnr
            _ssim=$(echo "$RD_VALS" | sed -n '1p')
            _psnr=$(echo "$RD_VALS" | sed -n '2p')
            if [ -n "$_ssim" ]; then SSIM="$_ssim"; fi
            if [ -n "$_psnr" ]; then PSNR="$_psnr"; fi
        else
            SSIM="N/A"
            PSNR="N/A"
        fi
        echo -e "$(basename "$file")\t$orig_size\t$tool_name\t$out_size\t${red}%\t${elapsed}\t${SSIM}\t${PSNR}\tOK" >> "$RESULTS_FILE"
    else
        echo -e "$(basename "$file")\t$orig_size\t$tool_name\tFAILED\tN/A\t${elapsed}\tN/A\tN/A\tFAILED" >> "$RESULTS_FILE"
    fi
}

echo "Running benchmarks..."
# Canonical 14-file corpus (mirrors kCanonicalFiles() in
# tests/test_corpus_verifier.cpp). The shared corpus directory also holds
# non-canonical fixtures (large_uncompressed, bitpacked_bpc1,
# distinct_duplicate_streams) and test byproducts, so iterate this explicit
# list instead of a raw *.pdf glob.
CANONICAL_FILES=(
    text_only.pdf
    photo_jpeg.pdf
    photo_heavy.pdf
    screenshot_flat.pdf
    line_art.pdf
    grayscale_scan.pdf
    monochrome_bw.pdf
    with_form.pdf
    with_bookmarks_and_links.pdf
    with_javascript.pdf
    with_metadata.pdf
    encrypted.pdf
    cmyk_image.pdf
    transparency.pdf
)
for base in "${CANONICAL_FILES[@]}"; do
    pdf="$CORPUS_DIR/$base"
    [ -e "$pdf" ] || continue
    # Defense in depth: keep the byproduct skip patterns in case the list is
    # ever widened back to a glob.
    if [[ "$pdf" == *"_optimized"* ]] || [[ "$pdf" == *".qpdf"* ]] || [[ "$pdf" == *".opt"* ]] || [[ "$pdf" == *".gs"* ]] || [[ "$pdf" == *".ocrmypdf"* ]]; then
        continue
    fi
    orig_size=$(stat -f%z "$pdf" 2>/dev/null || stat -c%s "$pdf")

    # 1. pdfcompress - never overwriting input, output is "${pdf%.pdf}_optimized.pdf" with || true for encrypted
    out_file="${pdf%.pdf}_optimized.pdf"
    run_tool "$pdf" "$orig_size" "pdfcompress" "(cd \"$BUILD_DIR\" && ./run_optimize \"$pdf\") || true" "$out_file"

    # 2. qpdf - valid recompress invocation, guarded by command -v qpdf
    out_file="${pdf%.pdf}.qpdf.pdf"
    if command -v qpdf >/dev/null 2>&1; then
        run_tool "$pdf" "$orig_size" "qpdf" "qpdf --recompress --compression-level=9 --object-streams=generate \"$pdf\" \"$out_file\"" "$out_file"
    else
        echo -e "$(basename "$pdf")\t$orig_size\tqpdf\tSKIPPED (not installed)\tN/A\t0\tN/A\tN/A\tSKIPPED (not installed)" >> "$RESULTS_FILE"
    fi

    # 3. gs ebook - guarded by command -v gs
    out_file="${pdf%.pdf}.gs_ebook.pdf"
    if command -v gs >/dev/null 2>&1; then
        run_tool "$pdf" "$orig_size" "gs_ebook" "gs -sDEVICE=pdfwrite -dPDFSETTINGS=/ebook -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile=\"$out_file\" \"$pdf\"" "$out_file"
    else
        echo -e "$(basename "$pdf")\t$orig_size\tgs_ebook\tSKIPPED (not installed)\tN/A\t0\tN/A\tN/A\tSKIPPED (not installed)" >> "$RESULTS_FILE"
    fi

    # 4. gs screen - guarded by command -v gs
    out_file="${pdf%.pdf}.gs_screen.pdf"
    if command -v gs >/dev/null 2>&1; then
        run_tool "$pdf" "$orig_size" "gs_screen" "gs -sDEVICE=pdfwrite -dPDFSETTINGS=/screen -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile=\"$out_file\" \"$pdf\"" "$out_file"
    else
        echo -e "$(basename "$pdf")\t$orig_size\tgs_screen\tSKIPPED (not installed)\tN/A\t0\tN/A\tN/A\tSKIPPED (not installed)" >> "$RESULTS_FILE"
    fi

    # 5. ocrmypdf - guarded by command -v ocrmypdf
    out_file="${pdf%.pdf}.ocrmypdf.pdf"
    if command -v ocrmypdf >/dev/null 2>&1; then
        run_tool "$pdf" "$orig_size" "ocrmypdf" "ocrmypdf --optimize 3 --skip-text \"$pdf\" \"$out_file\"" "$out_file"
    else
        echo -e "$(basename "$pdf")\t$orig_size\tocrmypdf\tSKIPPED (not installed)\tN/A\t0\tN/A\tN/A\tSKIPPED (not installed)" >> "$RESULTS_FILE"
    fi
done

cp "$RESULTS_FILE" "$BUILD_DIR"/ 2>/dev/null || true

echo "Benchmark complete. Results saved to $RESULTS_FILE"
cat "$RESULTS_FILE" | column -t -s $'\t'
