#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RAW_DIR="$REPO_ROOT/data/raw"

# Calculate md5 checksum of FILE and stores it in MD5_RESULT
function get_checksum() {
   FILE=$1

   if [ -x "$(command -v md5sum)" ]; then
      # Linux
      MD5_RESULT=`md5sum ${FILE} | awk '{ print $1 }'`
   else
      # OS X
      MD5_RESULT=`md5 -q ${FILE}`
   fi
}


function download_file_zst() {
   FILE=$1;
   CHECKSUM=$2;
   URL=$3;

   # Check if file already exists
   if [ -f ${FILE} ]; then
      # Exists -> check the checksum
      get_checksum ${FILE}
      if [ "${MD5_RESULT}" != "${CHECKSUM}" ]; then
         wget -O - ${URL} | zstd -d > ${FILE}
      fi
   else
      # Does not exists -> download
      wget -O - ${URL} | zstd -d > ${FILE}
   fi

   # Validate (at this point the file should really exist)
   get_checksum ${FILE}
   if [ "${MD5_RESULT}" != "${CHECKSUM}" ]; then
      echo "error checksum does not match: run download again"
      exit 1
   else
      echo ${FILE} "checksum ok"
   fi
}

# Datasets required by the experiment pipeline:
#   books_200M_uint64      (downsampled from books_800M_uint64)
#   osm_cellids_200M_uint64 (downsampled from osm_cellids_800M_uint64)
#   fb_200M_uint64         (downloaded directly)
REQUIRED_FILES=(
   books_200M_uint64
   fb_200M_uint64
   osm_cellids_200M_uint64
)

function all_required_present() {
   for f in "${REQUIRED_FILES[@]}"; do
      if [ ! -f "$RAW_DIR/$f" ]; then
         return 1
      fi
   done
   return 0
}

function main() {
   mkdir -p "$RAW_DIR"

   if all_required_present; then
      echo "Required datasets already present in $RAW_DIR; skipping download."
      for f in "${REQUIRED_FILES[@]}"; do
         ls -lh "$RAW_DIR/$f"
      done
      exit 0
   fi

   echo "downloading data ..."
   cd "$RAW_DIR"

   # Format: download_file_zst <file_name> <md5_checksum> <url>
   download_file_zst books_800M_uint64 8708eb3e1757640ba18dcd3a0dbb53bc https://www.dropbox.com/s/y2u3nbanbnbmg7n/books_800M_uint64.zst?dl=1
   download_file_zst osm_cellids_800M_uint64 70670bf41196b9591e07d0128a281b9a https://www.dropbox.com/s/j1d4ufn4fyb4po2/osm_cellids_800M_uint64.zst?dl=1
   download_file_zst fb_200M_uint64 3b0f820caa0d62150e87ce94ec989978 https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/EATHF7

   echo "done"
}

main

echo "Downsampling 800M -> 200M (books, osm_cellids)..."
python3 "$SCRIPT_DIR/downsample.py"

# Keep only the three files used by the experiments
cd "$RAW_DIR"
rm -f books_800M_uint64 osm_cellids_800M_uint64
echo "Kept in $RAW_DIR:"
ls -1 books_200M_uint64 osm_cellids_200M_uint64 fb_200M_uint64
