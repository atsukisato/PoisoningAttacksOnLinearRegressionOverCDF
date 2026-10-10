#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RAW_DIR="$REPO_ROOT/data/raw"

get_checksum() {
    if command -v md5sum >/dev/null 2>&1; then
        md5sum "$1" | awk '{print $1}'
    else
        md5 -q "$1"
    fi
}

download_file_zst() {
    local name="$1"
    local expected="$2"
    local url="$3"
    local output="$RAW_DIR/$name"
    local actual

    if [[ -f "$output" ]]; then
        echo "Checking $name ..."
        actual="$(get_checksum "$output")"
        if [[ "$actual" == "$expected" ]]; then
            echo "$name: checksum OK; skipping download."
            return 0
        fi
        echo "$name: checksum mismatch; downloading a replacement."
    fi

    echo "Downloading $name ..."
    TEMP_OUTPUT="$(mktemp "$RAW_DIR/$name.tmp.XXXXXX")"
    if ! wget -O - "$url" | zstd -d > "$TEMP_OUTPUT"; then
        echo "Error: download or decompression failed for $name." >&2
        rm -f "$TEMP_OUTPUT"
        TEMP_OUTPUT=""
        return 1
    fi

    echo "Verifying $name ..."
    actual="$(get_checksum "$TEMP_OUTPUT")"
    if [[ "$actual" != "$expected" ]]; then
        echo "Error: checksum mismatch for $name." >&2
        echo "Expected: $expected" >&2
        echo "Actual:   $actual" >&2
        rm -f "$TEMP_OUTPUT"
        TEMP_OUTPUT=""
        echo "Rerun this script to download it again." >&2
        return 1
    fi

    mv -f "$TEMP_OUTPUT" "$output"
    TEMP_OUTPUT=""
    echo "$name: checksum OK."
}

TEMP_OUTPUT=""
cleanup() {
    if [[ -n "$TEMP_OUTPUT" ]]; then
        rm -f "$TEMP_OUTPUT"
    fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

main() {
    local cmd
    for cmd in wget zstd; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            echo "Error: required command not found: $cmd" >&2
            exit 1
        fi
    done
    if ! command -v md5sum >/dev/null 2>&1 && \
       ! command -v md5 >/dev/null 2>&1; then
        echo "Error: md5sum (Linux) or md5 (macOS) is required." >&2
        exit 1
    fi

    mkdir -p "$RAW_DIR"

    download_file_zst books_200M_uint64 aeedc7be338399ced89d0bb82287e024 \
        'https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/A6HDNT'

    download_file_zst fb_200M_uint64 3b0f820caa0d62150e87ce94ec989978 \
        'https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/EATHF7'

    download_file_zst osm_cellids_200M_uint64 a7f6b8d2df09fcda5d9cfbc87d765979 \
        'https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/8FX9BV'

    echo "All datasets downloaded in $RAW_DIR"
}

main "$@"
