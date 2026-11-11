#!/usr/bin/env python3
"""Read-only candidates from Crisol's custom actor serialization (supported EXE only).

Local reports contain player state. Never commit reports or save files.
Payload bytes are intentionally unnamed: this tool cannot repair progression.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

CAP = 128 * 1024 * 1024
TARGETS = ('CR_Teleport_BP_C', 'CR_SunChestBox_BP_C', 'AcolyteSunchestAbsorb_BP_C')


def fstring(data: bytes, offset: int) -> tuple[str, int]:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError('Missing string length')
    size = struct.unpack_from('<i', data, offset)[0]
    end = offset + 4 + size
    if not 2 <= size <= 1024 or end > len(data) or data[end-1] != 0:
        raise ValueError('Invalid candidate FString')
    raw = data[offset+4:end-1]
    if not all(32 <= b <= 126 for b in raw):
        raise ValueError('Not a printable ASCII FString')
    return raw.decode('ascii'), end


def inspect(data: bytes) -> list[dict]:
    if len(data) > CAP:
        raise ValueError('Save exceeds safety cap')
    records = []
    offset = 0
    while True:
        pos = data.find(b'/Game/', offset)
        if pos < 0:
            break
        offset = pos + 6
        try:
            cls, p = fstring(data, pos-4)
            if not cls.endswith(TARGETS):
                continue
            owner, p = fstring(data, p)
            actor, p = fstring(data, p)
            if '.SaveComp.' not in owner or not actor or p + 88 > len(data):
                continue
            transform = struct.unpack_from('<10d', data, p)
            if not all(math.isfinite(v) for v in transform):
                continue
            if abs(sum(v*v for v in transform[:4])-1) > .01:
                continue
            marker, size = struct.unpack_from('<II', data, p+80)
            end = p + 88 + size
            if marker not in (0, 1) or size > 1024*1024 or end > len(data):
                continue
            records.append(dict(class_path=cls, owner=owner, actor=actor,
                record_offset=pos-4, position=list(transform[4:7]),
                marker_uninterpreted=marker, payload_offset=p+88,
                payload_size=size, payload_hex=data[p+88:end].hex(), record_end=end))
            offset = end
        except (ValueError, struct.error):
            continue
    return records


def achievements(data: bytes) -> dict:
    """Decode the two observed leading maps, retaining unparsed trailer bytes."""
    p = 0
    def integer():
        nonlocal p
        if p + 4 > len(data):
            raise ValueError('Truncated achievement integer')
        value = struct.unpack_from('<i', data, p)[0]
        p += 4
        return value
    def string():
        nonlocal p
        n = integer()
        width = 2 if n < 0 else 1
        size = abs(n) * width
        if size > 8192 or p + size > len(data):
            raise ValueError('Invalid achievement string')
        raw = data[p:p+size]
        p += size
        if not n:
            return ''
        if raw[-width:] != b'\0' * width:
            raise ValueError('Missing string terminator')
        return raw[:-width].decode('utf-16-le' if n < 0 else 'utf-8')
    def count():
        n = integer()
        if not 0 <= n <= 10000:
            raise ValueError('Invalid map/array count')
        return n
    counters, groups = {}, {}
    for _ in range(count()):
        key = string()
        if key in counters:
            raise ValueError('Duplicate counter')
        counters[key] = integer()
    for _ in range(count()):
        key = string()
        if key in groups:
            raise ValueError('Duplicate achievement group')
        groups[key] = [string() for _ in range(count())]
    return dict(counters=counters, groups=groups, parsed_bytes=p,
                unparsed_trailer_hex=data[p:].hex())


def load(path: Path) -> dict:
    if path.stat().st_size > CAP:
        raise ValueError('Save exceeds safety cap')
    data = path.read_bytes()
    report = dict(file=path.name, size=len(data), sha256=hashlib.sha256(data).hexdigest(),
        warning='Candidate record layout only. Payload meaning is not established. No repair.',
        records=inspect(data))
    if path.name.casefold() == 'achievementdata.sav':
        report['achievement_data'] = achievements(data)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('save', type=Path)
    parser.add_argument('--compare', type=Path)
    args = parser.parse_args()
    report = load(args.save)
    if args.compare:
        other = load(args.compare)
        before = {r['owner']: r for r in report['records']}
        after = {r['owner']: r for r in other['records']}
        report['comparison_file'] = other['file']
        report['comparison_sha256'] = other['sha256']
        report['payload_changes'] = [dict(owner=k,
            before=before.get(k, {}).get('payload_hex'),
            after=after.get(k, {}).get('payload_hex'))
            for k in sorted(before.keys() | after.keys())
            if before.get(k, {}).get('payload_hex') != after.get(k, {}).get('payload_hex')]
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
