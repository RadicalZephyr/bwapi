#!/usr/bin/env python3
"""Decode a Blizzard BLTE container and parse a TACT 'IN' install manifest.

Used to determine which files a Blizzard product actually installs to disk,
without installing it. See docs/starcraft-linux-local-dev.md, Appendix A, for
how to obtain an install manifest from the public CDN.

Supports BLTE chunk modes N (raw), Z (zlib) and F (recursive frame).
Mode E (Salsa20) is not supported; install manifests do not use it.
"""

import argparse
import signal
import struct
import sys
import zlib

# Listings are long and routinely piped into head/grep; die quietly on SIGPIPE
# instead of dumping a BrokenPipeError traceback.
signal.signal(signal.SIGPIPE, signal.SIG_DFL)


def blte_decode(data):
    if data[:4] != b'BLTE':
        raise ValueError(f"not a BLTE container (magic={data[:4]!r})")
    header_size = struct.unpack('>I', data[4:8])[0]
    if header_size == 0:
        return decode_chunk(data[8:])

    chunk_count = (data[9] << 16) | (data[10] << 8) | data[11]
    pos = 12
    chunks = []
    for _ in range(chunk_count):
        csize, dsize = struct.unpack('>II', data[pos:pos + 8])
        pos += 8 + 16  # skip the 16-byte md5 checksum
        chunks.append((csize, dsize))

    out = bytearray()
    pos = header_size
    for csize, _dsize in chunks:
        out += decode_chunk(data[pos:pos + csize])
        pos += csize
    return bytes(out)


def decode_chunk(chunk):
    mode, body = chunk[0:1], chunk[1:]
    if mode == b'N':
        return body
    if mode == b'Z':
        return zlib.decompress(body)
    if mode == b'F':
        return blte_decode(body)
    if mode == b'E':
        raise NotImplementedError("encrypted chunk: needs a TACT key")
    raise ValueError(f"unknown BLTE chunk mode {mode!r}")


def parse_install(buf):
    """Parse a TACT install manifest. Returns (version, tags, entries)."""
    if buf[:2] != b'IN':
        raise ValueError(f"not an install manifest (magic={buf[:2]!r})")
    version, hash_size = buf[2], buf[3]
    num_tags, num_entries = struct.unpack('>HI', buf[4:10])
    pos = 10

    mask_len = (num_entries + 7) // 8
    tags = []
    for _ in range(num_tags):
        end = buf.index(b'\x00', pos)
        name = buf[pos:end].decode('utf-8', 'replace')
        pos = end + 1
        ttype = struct.unpack('>H', buf[pos:pos + 2])[0]
        pos += 2 + mask_len  # skip the per-entry bitmask
        tags.append((name, ttype))

    entries = []
    for _ in range(num_entries):
        end = buf.index(b'\x00', pos)
        name = buf[pos:end].decode('utf-8', 'replace')
        pos = end + 1
        chash = buf[pos:pos + hash_size].hex()
        pos += hash_size
        size = struct.unpack('>I', buf[pos:pos + 4])[0]
        pos += 4
        entries.append((name, chash, size))

    return version, tags, entries


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('manifest', help='BLTE-encoded install manifest')
    ap.add_argument('--all', action='store_true', help='list every entry')
    ap.add_argument('--grep', metavar='SUBSTR',
                    help='list entries whose path contains SUBSTR (case-insensitive)')
    ap.add_argument('--top', type=int, default=20, metavar='N',
                    help='show the N largest entries (default 20)')
    args = ap.parse_args()

    raw = open(args.manifest, 'rb').read()
    decoded = blte_decode(raw)
    version, tags, entries = parse_install(decoded)

    print(f"decoded {len(raw):,} -> {len(decoded):,} bytes")
    print(f"install manifest v{version}: {len(tags)} tags, {len(entries)} entries, "
          f"{sum(e[2] for e in entries):,} bytes total")
    print(f"\ntags: {', '.join(t[0] for t in tags)}")

    if args.all:
        selected, label = entries, "all entries"
    elif args.grep:
        needle = args.grep.lower()
        selected = [e for e in entries if needle in e[0].lower()]
        label = f"entries matching {args.grep!r}"
    else:
        # Default view: the question this tool was written to answer.
        selected = [e for e in entries if e[0].lower().endswith('.mpq')]
        label = "MPQ entries"

    print(f"\n=== {label}: {len(selected)} ===")
    for name, _chash, size in sorted(selected, key=lambda e: -e[2]):
        print(f"  {size:>12,}  {name}")
    if not selected:
        print("  (none)")

    if not (args.all or args.grep):
        print(f"\n=== {args.top} largest entries ===")
        for name, _chash, size in sorted(entries, key=lambda e: -e[2])[:args.top]:
            print(f"  {size:>12,}  {name}")


if __name__ == '__main__':
    sys.exit(main())
