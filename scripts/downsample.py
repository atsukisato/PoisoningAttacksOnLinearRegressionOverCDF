#!/usr/bin/env python3
"""Downsample SOSD 800M uint64 datasets to 200M for the experiment pipeline."""

import os
import struct

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
RAW_DIR = os.path.join(REPO_ROOT, "data", "raw")

CHUNK_KEYS = 4 * 1024 * 1024  # multiple of 4; ~32 MiB


def downsample_to_200m(name: str) -> None:
    src = os.path.join(RAW_DIR, f"{name}_800M_uint64")
    dst = os.path.join(RAW_DIR, f"{name}_200M_uint64")

    if os.path.exists(dst):
        print(f"Skip {name}: {dst} already exists")
        return

    if not os.path.exists(src):
        raise FileNotFoundError(f"Missing source dataset: {src}")

    print(f"Downsampling {name}: 800M -> 200M")
    keys_written = 0

    with open(src, "rb") as fin, open(dst, "wb") as fout:
        header = fin.read(8)
        if len(header) != 8:
            raise ValueError(f"Truncated header in {src}")
        count = struct.unpack("Q", header)[0]

        fout.write(struct.pack("Q", 0))

        remaining = count
        while remaining > 0:
            n_read = min(remaining, CHUNK_KEYS)
            data = np.fromfile(fin, dtype=np.uint64, count=n_read)
            if len(data) != n_read:
                raise ValueError(
                    f"Unexpected EOF in {src}: expected {n_read} keys, got {len(data)}"
                )
            selected = data[::4]
            selected.tofile(fout)
            keys_written += len(selected)
            remaining -= n_read

        fout.seek(0)
        fout.write(struct.pack("Q", keys_written))

    print(f"Wrote {dst} ({keys_written} keys)")


if __name__ == "__main__":
    downsample_to_200m("books")
    downsample_to_200m("osm_cellids")
