#!/usr/bin/env bash
set -euo pipefail

# Compare local data/generated and results against extracted reference trees.

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

REF_DATA_GENERATED="$PROJECT_ROOT/reference/data/generated"
REF_RESULTS="$PROJECT_ROOT/reference/results"
COMPARISON_DIR="$PROJECT_ROOT/comparison"

echo "Step 8: Comparing against reference..."
echo "  Source generated: $DATA_DIR"
echo "  Source results:   $RESULTS_DIR"
echo "  Reference data:   $REF_DATA_GENERATED"
echo "  Reference results:$REF_RESULTS"
echo "  Reports:          $COMPARISON_DIR"

missing=0
if [ ! -d "$REF_DATA_GENERATED" ]; then
    echo "error: missing extracted reference data: $REF_DATA_GENERATED" >&2
    echo "error: run scripts/prepare_reference.sh first" >&2
    missing=1
fi
if [ ! -d "$REF_RESULTS" ]; then
    echo "error: missing extracted reference results: $REF_RESULTS" >&2
    echo "error: run scripts/prepare_reference.sh first" >&2
    missing=1
fi
if [ "$missing" -ne 0 ]; then
    exit 1
fi

set +e
python3 "$SCRIPT_DIR/compare_reference.py" \
    --source "$PROJECT_ROOT" \
    --reference-data-generated "$REF_DATA_GENERATED" \
    --reference-results "$REF_RESULTS" \
    --output-dir "$COMPARISON_DIR"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "Step 8: comparison PASS (no differences)."
    exit 0
fi

if [ "$status" -eq 1 ]; then
    echo "Step 8: comparison DIFF (see $COMPARISON_DIR/summary.txt)."
    echo "Differences can occur (e.g. random poison, floating-point, environment)."
    # Report written; do not fail the pipeline solely because of value differences.
    exit 0
fi

echo "error: comparison failed with exit status $status" >&2
exit "$status"
