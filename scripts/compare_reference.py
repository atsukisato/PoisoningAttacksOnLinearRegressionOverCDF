#!/usr/bin/env python3
"""Compare source data/results trees against reference directories."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Iterator, Literal, TextIO

CHUNK_SIZE = 1024 * 1024  # 1 MiB

# Machine/run logs excluded from reference results
IGNORED_BASENAMES = frozenset(
    {
        ".DS_Store",
        "performance_setup.log",
        "run_all.log",
        "system_info.log",
    }
)

CompareMode = Literal["bytes", "results"]

try:
    from tqdm import tqdm
except ImportError:  # pragma: no cover
    tqdm = None  # type: ignore[assignment]


RESULTS_FAMILIES = (
    "inject",
    "loss",
    "optimal_poison",
    "upper_bound",
)


def _results_family(rel: str) -> str | None:
    top = rel.split("/", 1)[0]
    if top.startswith("inject_poison"):
        return "inject"
    if top.startswith("loss"):
        return "loss"
    if top.startswith("optimal_poison"):
        return "optimal_poison"
    if top.startswith("upper_bound"):
        return "upper_bound"
    return None


def _results_field_key(rel: str) -> str | None:
    """Return JSON field to compare, 'full_json', or None if path is excluded."""
    family = _results_family(rel)
    if family == "inject":
        return "mse"
    if family == "loss":
        return "full_json"
    if family == "optimal_poison":
        return "loss"
    if family == "upper_bound":
        return "mse_upper_bound"
    return None


def _should_ignore(path: Path, *, mode: CompareMode = "bytes") -> bool:
    parts = path.parts
    if any(part.startswith(".") for part in parts):
        return True
    if "__MACOSX" in parts:
        return True
    if "figures" in parts:
        return True
    if path.name in IGNORED_BASENAMES:
        return True
    if mode == "results":
        # Only compare known result families; skip query_time / tables / others.
        if _results_field_key(path.as_posix()) is None:
            return True
    return False


def iter_relative_files(root: Path, *, mode: CompareMode = "bytes") -> set[str]:
    if not root.is_dir():
        return set()
    files: set[str] = set()
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(root)
        if _should_ignore(rel, mode=mode):
            continue
        files.add(rel.as_posix())
    return files


def file_size(path: Path) -> int:
    return path.stat().st_size


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _load_json(path: Path) -> Any | None:
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return None


def files_match_bytes(src_path: Path, ref_path: Path) -> bool:
    if file_size(src_path) != file_size(ref_path):
        return False
    return sha256_file(src_path) == sha256_file(ref_path)


def json_values_match(
    a: Any,
    b: Any,
    *,
    rtol: float,
    atol: float,
) -> bool:
    if isinstance(a, bool) or isinstance(b, bool):
        return type(a) is type(b) and a == b

    if isinstance(a, int) and isinstance(b, int):
        return a == b

    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if not math.isfinite(a) or not math.isfinite(b):
            return False
        return math.isclose(a, b, rel_tol=rtol, abs_tol=atol)

    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(
            json_values_match(a[k], b[k], rtol=rtol, atol=atol)
            for k in a
        )

    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(
            json_values_match(x, y, rtol=rtol, atol=atol)
            for x, y in zip(a, b)
        )

    return type(a) is type(b) and a == b


def files_match_results(rel: str, src_path: Path, ref_path: Path) -> bool:
    key = _results_field_key(rel)
    if key is None:
        return False

    src_obj = _load_json(src_path)
    ref_obj = _load_json(ref_path)
    if not isinstance(src_obj, dict) or not isinstance(ref_obj, dict):
        return False

    rtol = 1e-6
    atol = 1e-12

    if key == "full_json":
        return json_values_match(src_obj, ref_obj, rtol=rtol, atol=atol)

    if key not in src_obj or key not in ref_obj:
        return False

    return json_values_match(
        src_obj[key], ref_obj[key], rtol=rtol, atol=atol
    )


@dataclass
class PairResult:
    name: str
    source_dir: Path
    reference_dir: Path
    source_only: list[str] = field(default_factory=list)
    reference_only: list[str] = field(default_factory=list)
    matched: list[str] = field(default_factory=list)
    mismatched: list[str] = field(default_factory=list)

    @property
    def source_count(self) -> int:
        return len(self.source_only) + len(self.matched) + len(self.mismatched)

    @property
    def reference_count(self) -> int:
        return len(self.reference_only) + len(self.matched) + len(self.mismatched)

    @property
    def has_diff(self) -> bool:
        return bool(self.source_only or self.reference_only or self.mismatched)


def _progress(iterable: Iterable[str], total: int, desc: str) -> Iterator[str]:
    if tqdm is not None:
        yield from tqdm(iterable, total=total, desc=desc, unit="file")
        return
    # Fallback: simple progress on stderr
    n = 0
    for item in iterable:
        n += 1
        if total == 0 or n == 1 or n == total or n % max(1, total // 50) == 0:
            pct = 100.0 * n / total if total else 100.0
            print(f"\r{desc}: {n}/{total} ({pct:.1f}%)", end="", file=sys.stderr, flush=True)
        yield item
    if total:
        print(file=sys.stderr)


def _banner(title: str, *, char: str = "=", file: TextIO = sys.stdout) -> None:
    bar = char * 72
    print(file=file)
    print(bar, file=file)
    print(f"  {title}", file=file)
    print(bar, file=file)


def compare_pair(
    name: str,
    source_dir: Path,
    reference_dir: Path,
    *,
    mode: CompareMode = "bytes",
) -> PairResult:
    result = PairResult(name=name, source_dir=source_dir, reference_dir=reference_dir)

    source_files = iter_relative_files(source_dir, mode=mode)
    reference_files = iter_relative_files(reference_dir, mode=mode)

    _banner(name)
    src_note = "" if source_dir.is_dir() else "  [missing, treated as empty]"
    ref_note = "" if reference_dir.is_dir() else "  [missing, treated as empty]"
    print(f"source:     {source_dir}  ({len(source_files)} files){src_note}")
    print(f"reference:  {reference_dir}  ({len(reference_files)} files){ref_note}")

    result.source_only = sorted(source_files - reference_files)
    result.reference_only = sorted(reference_files - source_files)
    common = sorted(source_files & reference_files)

    for rel in _progress(common, total=len(common), desc=f"compare {name}"):
        src_path = source_dir / rel
        ref_path = reference_dir / rel
        if mode == "results":
            ok = files_match_results(rel, src_path, ref_path)
        else:
            ok = files_match_bytes(src_path, ref_path)
        if ok:
            result.matched.append(rel)
        else:
            result.mismatched.append(rel)

    return result


def _count_by_family(paths: list[str], family: str) -> int:
    return sum(1 for p in paths if _results_family(p) == family)


def _format_four_counts(
    source_only: int,
    reference_only: int,
    matched: int,
    mismatched: int,
    *,
    indent: str = "  ",
) -> list[str]:
    return [
        f"{indent}source only:              {source_only}",
        f"{indent}reference only:           {reference_only}",
        f"{indent}both and match:           {matched}",
        f"{indent}both and mismatch:        {mismatched}",
    ]


def _print_four_counts(
    source_only: int,
    reference_only: int,
    matched: int,
    mismatched: int,
    *,
    indent: str = "  ",
    file: TextIO = sys.stdout,
) -> None:
    for line in _format_four_counts(
        source_only, reference_only, matched, mismatched, indent=indent
    ):
        print(line, file=file)


def print_pair_report(result: PairResult, *, file: TextIO = sys.stdout) -> None:
    _banner(f"SUMMARY: {result.name}", char="-", file=file)
    if result.name != "results":
        _print_four_counts(
            len(result.source_only),
            len(result.reference_only),
            len(result.matched),
            len(result.mismatched),
            file=file,
        )
        return

    for family in RESULTS_FAMILIES:
        print(f"\n  [{family}]", file=file)
        _print_four_counts(
            _count_by_family(result.source_only, family),
            _count_by_family(result.reference_only, family),
            _count_by_family(result.matched, family),
            _count_by_family(result.mismatched, family),
            indent="    ",
            file=file,
        )


def print_mismatch_names(results: list[PairResult], *, file: TextIO = sys.stdout) -> None:
    """List both-and-mismatch paths only at the end (names only)."""
    entries: list[str] = []
    for result in results:
        for rel in result.mismatched:
            entries.append(f"{result.name}/{rel}")
    _banner("BOTH AND MISMATCH", file=file)
    if not entries:
        print("  (none)", file=file)
        return
    for path in entries:
        print(f"  {path}", file=file)


def _prefixed_paths(results: list[PairResult], attr: str) -> list[str]:
    entries: list[str] = []
    for result in results:
        for rel in getattr(result, attr):
            entries.append(f"{result.name}/{rel}")
    return entries


def write_report_files(output_dir: Path, results: list[PairResult]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    any_diff = any(r.has_diff for r in results)
    status = "DIFF" if any_diff else "PASS"

    summary_lines: list[str] = [
        f"status: {status}",
        "",
    ]
    for result in results:
        summary_lines.append(f"[{result.name}]")
        summary_lines.append(f"  source:     {result.source_dir}")
        summary_lines.append(f"  reference:  {result.reference_dir}")
        if result.name == "results":
            for family in RESULTS_FAMILIES:
                summary_lines.append(f"  [{family}]")
                summary_lines.extend(
                    _format_four_counts(
                        _count_by_family(result.source_only, family),
                        _count_by_family(result.reference_only, family),
                        _count_by_family(result.matched, family),
                        _count_by_family(result.mismatched, family),
                        indent="    ",
                    )
                )
        else:
            summary_lines.extend(
                _format_four_counts(
                    len(result.source_only),
                    len(result.reference_only),
                    len(result.matched),
                    len(result.mismatched),
                )
            )
        summary_lines.append("")

    if len(results) > 1:
        summary_lines.append("[overall]")
        summary_lines.extend(
            _format_four_counts(
                sum(len(r.source_only) for r in results),
                sum(len(r.reference_only) for r in results),
                sum(len(r.matched) for r in results),
                sum(len(r.mismatched) for r in results),
            )
        )
        summary_lines.append("")

    (output_dir / "summary.txt").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    mismatches = _prefixed_paths(results, "mismatched")
    source_only = _prefixed_paths(results, "source_only")
    reference_only = _prefixed_paths(results, "reference_only")

    def _write_list(path: Path, entries: list[str]) -> None:
        body = "\n".join(entries) + ("\n" if entries else "")
        path.write_text(body, encoding="utf-8")

    _write_list(output_dir / "mismatch_file_paths.txt", mismatches)
    _write_list(output_dir / "source_only_file_paths.txt", source_only)
    _write_list(output_dir / "reference_only_file_paths.txt", reference_only)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare source data/generated and results "
            "against reference directories."
        )
    )
    parser.add_argument(
        "--source",
        required=True,
        type=Path,
        help="Source code root (contains data/generated, results)",
    )
    parser.add_argument(
        "--reference-data-generated",
        type=Path,
        default=None,
        help="Path to reference_data_generated (compared with SOURCE/data/generated)",
    )
    parser.add_argument(
        "--reference-results",
        type=Path,
        default=None,
        help="Path to reference_results (compared with SOURCE/results)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help=(
            "Write summary.txt, mismatch_file_paths.txt, "
            "source_only_file_paths.txt, reference_only_file_paths.txt here"
        ),
    )
    args = parser.parse_args(argv)
    if not any(
        [
            args.reference_data_generated,
            args.reference_results,
        ]
    ):
        parser.error(
            "specify at least one of "
            "--reference-data-generated, --reference-results"
        )
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    source_root = args.source.resolve()
    if not source_root.is_dir():
        print(f"error: source root not found: {source_root}", file=sys.stderr)
        return 2

    pairs: list[tuple[str, Path, Path, CompareMode]] = []
    if args.reference_data_generated is not None:
        pairs.append(
            (
                "data/generated",
                source_root / "data" / "generated",
                args.reference_data_generated.resolve(),
                "bytes",
            )
        )
    if args.reference_results is not None:
        pairs.append(
            (
                "results",
                source_root / "results",
                args.reference_results.resolve(),
                "results",
            )
        )

    results: list[PairResult] = []
    for name, src_dir, ref_dir, mode in pairs:
        pair_result = compare_pair(name, src_dir, ref_dir, mode=mode)
        print_pair_report(pair_result)
        results.append(pair_result)

    if len(results) > 1:
        _banner("OVERALL SUMMARY")
        total_src_only = sum(len(r.source_only) for r in results)
        total_ref_only = sum(len(r.reference_only) for r in results)
        total_match = sum(len(r.matched) for r in results)
        total_mismatch = sum(len(r.mismatched) for r in results)
        print(f"  source only:              {total_src_only}")
        print(f"  reference only:           {total_ref_only}")
        print(f"  both and match:           {total_match}")
        print(f"  both and mismatch:        {total_mismatch}")

    print_mismatch_names(results)

    if args.output_dir is not None:
        output_dir = args.output_dir.resolve()
        write_report_files(output_dir, results)
        print(f"\nWrote comparison reports to: {output_dir}")

    any_diff = any(r.has_diff for r in results)
    return 1 if any_diff else 0


if __name__ == "__main__":
    raise SystemExit(main())
