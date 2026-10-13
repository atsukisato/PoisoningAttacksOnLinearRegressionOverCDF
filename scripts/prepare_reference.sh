#!/usr/bin/env bash
set -euo pipefail

# Verify reference ZIP checksums and extract under reference/{data/generated,results}/.

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

ARCHIVES_DIR="$PROJECT_ROOT/reference/archives"
REF_DATA_GENERATED="$PROJECT_ROOT/reference/data/generated"
REF_RESULTS="$PROJECT_ROOT/reference/results"
STAGING_DIR="$PROJECT_ROOT/reference/.extract_tmp"

ZIP_GENERATED="reference-data-generated.zip"
ZIP_RESULTS="reference-results.zip"
SUMS_FILE="SHA256SUMS.txt"

echo "Preparing reference archives..."
echo "  Archives directory: $ARCHIVES_DIR"

if [ -d "$REF_DATA_GENERATED" ] && [ -d "$REF_RESULTS" ]; then
    echo "Reference already extracted; skipping."
    echo "  Generated data: $REF_DATA_GENERATED"
    echo "  Results:        $REF_RESULTS"
    exit 0
fi

if [ ! -f "$ARCHIVES_DIR/$SUMS_FILE" ]; then
    echo "error: missing checksum file: $ARCHIVES_DIR/$SUMS_FILE" >&2
    echo "error: this file is shipped with the repository under reference/archives/" >&2
    exit 1
fi

missing=0
for f in "$ZIP_GENERATED" "$ZIP_RESULTS"; do
    if [ ! -f "$ARCHIVES_DIR/$f" ]; then
        echo "error: missing required file: $ARCHIVES_DIR/$f" >&2
        missing=1
    fi
done
if [ "$missing" -ne 0 ]; then
    echo "error: download $ZIP_GENERATED and $ZIP_RESULTS from Zenodo into reference/archives/" >&2
    exit 1
fi

echo "Verifying SHA-256 checksums..."
(
    cd "$ARCHIVES_DIR"
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 -c "$SUMS_FILE"
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum -c "$SUMS_FILE"
    else
        echo "error: neither shasum nor sha256sum is available" >&2
        exit 1
    fi
)

echo "Extracting reference archives..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

extract_zip() {
    local zip_path="$1"
    local dest_dir="$2"
    python3 - "$zip_path" "$dest_dir" <<'PY'
import sys
import zipfile
from pathlib import Path

zf_path, dest = Path(sys.argv[1]), Path(sys.argv[2])
with zipfile.ZipFile(zf_path) as zf:
    zf.extractall(dest)
PY
}

extract_zip "$ARCHIVES_DIR/$ZIP_GENERATED" "$STAGING_DIR"
extract_zip "$ARCHIVES_DIR/$ZIP_RESULTS" "$STAGING_DIR"

if [ ! -d "$STAGING_DIR/reference_data_generated" ]; then
    echo "error: expected top-level directory reference_data_generated/ in $ZIP_GENERATED" >&2
    rm -rf "$STAGING_DIR"
    exit 1
fi
if [ ! -d "$STAGING_DIR/reference_results" ]; then
    echo "error: expected top-level directory reference_results/ in $ZIP_RESULTS" >&2
    rm -rf "$STAGING_DIR"
    exit 1
fi

rm -rf "$REF_DATA_GENERATED" "$REF_RESULTS"
mkdir -p "$(dirname "$REF_DATA_GENERATED")"
mv "$STAGING_DIR/reference_data_generated" "$REF_DATA_GENERATED"
mv "$STAGING_DIR/reference_results" "$REF_RESULTS"
rm -rf "$STAGING_DIR"

echo "Reference prepared:"
echo "  Generated data: $REF_DATA_GENERATED"
echo "  Results:        $REF_RESULTS"
