#!/usr/bin/env bash
set -euo pipefail

# [PAPER] Step 7: Compile the paper (main.tex -> main.pdf)
# Can be run standalone or as the final step of the comprehensive experiment.

# Load common environment
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

echo "[PAPER] Step 7: Compiling paper..."
echo "  Paper directory: $PAPER_DIR"
echo ""

if [ ! -d "$PAPER_DIR" ]; then
    echo "[ERROR] Paper directory not found: $PAPER_DIR"
    exit 1
fi

if [ ! -f "$PAPER_DIR/main.tex" ]; then
    echo "[ERROR] main.tex not found in: $PAPER_DIR"
    exit 1
fi

# Copy experiment figures required by main.tex into the paper tree
PAPER_FIG_DIR="$PAPER_DIR/fig/experiments"
mkdir -p "$PAPER_FIG_DIR"

EXPERIMENT_FIGS=(
    "$RESULTS_DIR/figures/lambda_Ls_seed0/seed0_all.pdf"
    "$RESULTS_DIR/figures/lambda_LgrLub/lambda_Lgr_divided_by_Lub_all.pdf"
    "$RESULTS_DIR/figures/lambda_LgrLconsec/lambda_Lgr_divided_by_Lconsec_all.pdf"
    "$RESULTS_DIR/figures/lambda_Lconsec_using_relaxed_solution_Lconsec/lambda_Lconsec_using_relaxed_solution_divided_by_Lconsec_all.pdf"
    "$RESULTS_DIR/figures/lambda_time/lambda_time_all.pdf"
    "$RESULTS_DIR/figures/n_time/n_time_all.pdf"
    "$RESULTS_DIR/figures/n_LgrLub/n_LgrLub_all.pdf"
    "$RESULTS_DIR/figures/R_LgrLub/R_LgrLub_all.pdf"
    "$RESULTS_DIR/figures/lambda_L_ratios_brute_force/lambda_L_ratios_brute_force_all.pdf"
    "$RESULTS_DIR/figures/R_L_ratios_brute_force/R_L_ratios_brute_force_all.pdf"
    "$RESULTS_DIR/figures/lambda_query_time/lambda_query_time_all.pdf"
)

echo "[PAPER] Copying experiment figures to: $PAPER_FIG_DIR"
for src in "${EXPERIMENT_FIGS[@]}"; do
    if [ ! -f "$src" ]; then
        echo "[ERROR] Required experiment figure not found: $src"
        echo "  Run step6_generate_plots.sh first."
        exit 1
    fi
    cp -f "$src" "$PAPER_FIG_DIR/"
    echo "  copied: $(basename "$src")"
done
echo ""

# Copy experiment tables required by main.tex into the paper tree
PAPER_TABLE_DIR="$PAPER_DIR/table"
mkdir -p "$PAPER_TABLE_DIR"

EXPERIMENT_TABLES=(
    "$RESULTS_DIR/tables/overview.tex"
)

echo "[PAPER] Copying experiment tables to: $PAPER_TABLE_DIR"
for src in "${EXPERIMENT_TABLES[@]}"; do
    if [ ! -f "$src" ]; then
        echo "[ERROR] Required experiment table not found: $src"
        echo "  Run step6_generate_plots.sh first."
        exit 1
    fi
    cp -f "$src" "$PAPER_TABLE_DIR/"
    echo "  copied: $(basename "$src")"
done
echo ""

cd "$PAPER_DIR"

# Drop host/Docker-absolute-path latexmk state that breaks remounted builds
echo "[PAPER] Cleaning previous LaTeX build artifacts..."
rm -f \
  main.aux main.bbl main.blg main.fls main.fdb_latexmk \
  main.log main.out main.synctex.gz main.pdf comment.cut
echo ""

if command -v latexmk >/dev/null 2>&1; then
    echo "  Using latexmk..."
    latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
elif command -v pdflatex >/dev/null 2>&1 && command -v bibtex >/dev/null 2>&1; then
    echo "  latexmk not found; falling back to pdflatex + bibtex..."
    pdflatex -interaction=nonstopmode -halt-on-error main.tex
    bibtex main
    pdflatex -interaction=nonstopmode -halt-on-error main.tex
    pdflatex -interaction=nonstopmode -halt-on-error main.tex
else
    echo "[ERROR] Neither latexmk nor pdflatex+bibtex is available."
    echo "Install TeX Live (e.g. texlive-latex-extra, texlive-bibtex-extra, latexmk) and retry."
    exit 1
fi

if [ ! -f "$PAPER_DIR/main.pdf" ]; then
    echo "[ERROR] Compilation finished but main.pdf was not produced."
    exit 1
fi

echo "[OK] Paper compilation completed"
echo "  PDF: $PAPER_DIR/main.pdf"
echo ""
