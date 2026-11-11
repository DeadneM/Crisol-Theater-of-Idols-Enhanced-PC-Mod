#!/usr/bin/env python3
"""Read-only first-pass inspector for Crisol .sav files.

This tool does NOT parse or edit progression flags. It scans for serialized
Unreal-style names and compares backups to help identify the real save format.
Only Python's standard library is required.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import sys
from collections import Counter
from pathlib import Path

DEFAULT_DIR = Path(os.environ.get("LOCALAPPDATA", "")) / "CRToiPrototype" / "Saved" / "SaveGames"
TOKENS = (
    "Solari", "Brooch", "Insign", "Badge", "Crest", "Chest", "Cofre",
    "Fair", "Tormentosa", "Festival", "Gate", "Door", "Locked", "Unlock",
    "Mermaid", "Siren", "RamDistrict", "AnchorDistrict", "Chapter",
    "Collected", "Collection", "Inventory", "Visited", "Opened",
)
MAX_FILE_BYTES = 128 * 1024 * 1024


def read_save(path: Path) -> bytes:
    size = path.stat().st_size
    if size > MAX_FILE_BYTES:
        raise ValueError(f"File exceeds safety cap ({size:,} bytes)")
    with path.open("rb") as handle:
        return handle.read()


def find_terms(data: bytes) -> list[tuple[int, str, str]]:
    matches: list[tuple[int, str, str]] = []
    for token in TOKENS:
        for encoding in ("ascii", "utf-16-le"):
            needle = token.encode(encoding)
            # Byte regex is ASCII case insensitive, including letters in
            # UTF-16LE (zero bytes remain exact).
            pattern = re.compile(re.escape(needle), re.IGNORECASE)
            for m in pattern.finditer(data):
                matches.append((m.start(), token, encoding))
    return sorted(set(matches))


def print_report(path: Path, data: bytes, maximum: int) -> None:
    print(f"\nFile: {path.name} ({len(data):,} bytes)")
    print("SHA-256:", hashlib.sha256(data).hexdigest())
    print("Header:", data[:24].hex(" "))
    print("GVAS magic:", "yes" if data[:4] == b"GVAS" else "not detected")
    matches = find_terms(data)
    counts = Counter((term, codec) for _, term, codec in matches)
    if not matches:
        print("No matching plain strings. Save might use IDs, compression, or encryption.")
    else:
        print(f"Candidate term appearances: {len(matches)}")
        for (term, codec), count in sorted(counts.items()):
            print(f"  {term} [{codec}]: {count}")
        for offset, term, codec in matches[:maximum]:
            print(f"  0x{offset:08X}: {term} ({codec})")
        if len(matches) > maximum:
            print(f"  ... {len(matches) - maximum} more hidden by --max-hits")
    print("NOTE: A string match is NOT proof of an item, flag or its current value.")


def compare_bytes(a: bytes, b: bytes, window: int = 4096) -> None:
    print(f"\nRead-only comparison: {len(a):,} vs {len(b):,} bytes")
    changed = 0
    ranges: list[tuple[int, int]] = []
    for start in range(0, max(len(a), len(b)), window):
        left, right = a[start:start + window], b[start:start + window]
        if left != right:
            changed += 1
            if len(ranges) < 50:
                ranges.append((start, min(max(len(a), len(b)), start + window)))
    print(f"Different {window}-byte blocks: {changed}")
    for start, end in ranges:
        print(f"  changed block: 0x{start:X}..0x{end:X}")
    if changed > len(ranges):
        print(f"  ... {changed - len(ranges)} more changed blocks")
    print("NOTE: A large delta can reflect compression, not individual progress flags.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("saves", nargs="*", type=Path,
                        help="Optional .sav files; defaults to all local Crisol .sav files")
    parser.add_argument("--compare", nargs=2, type=Path, metavar=("BEFORE", "AFTER"),
                        help="Compare two backups by changed blocks (read only)")
    parser.add_argument("--max-hits", type=int, default=40,
                        help="Limit printed term offsets per save (default 40)")
    args = parser.parse_args()
    paths = args.saves or sorted(DEFAULT_DIR.glob("*.sav"))
    if args.compare:
        paths = list(dict.fromkeys([*paths, *args.compare]))
    if not paths:
        print("No .sav found in", DEFAULT_DIR)
        return 2
    cache: dict[Path, bytes] = {}
    for path in paths:
        try:
            data = read_save(path)
        except (OSError, ValueError) as error:
            print(f"Cannot read {path.name}: {error}", file=sys.stderr)
            continue
        cache[path] = data
        print_report(path, data, max(0, min(args.max_hits, 500)))
    if args.compare and all(p in cache for p in args.compare):
        compare_bytes(cache[args.compare[0]], cache[args.compare[1]])
    return 0 if cache else 2


if __name__ == "__main__":
    raise SystemExit(main())
