#!/usr/bin/env python3
"""Pack the user-space binaries from build/bin into build/apps_blob.bin.

Record format (little-endian):
    u32 name_len, u32 data_len, name_bytes, data_bytes
Terminated by a record with name_len == 0 and data_len == 0.
"""
import os
import struct
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: make_apps_blob.py <bindir> <outfile>", file=sys.stderr)
        return 1
    bindir, outfile = sys.argv[1], sys.argv[2]

    names = sorted(
        n for n in os.listdir(bindir)
        if os.path.isfile(os.path.join(bindir, n))
    )

    with open(outfile, "wb") as f:
        for name in names:
            with open(os.path.join(bindir, name), "rb") as src:
                data = src.read()
            name_bytes = name.encode("utf-8")
            f.write(struct.pack("<II", len(name_bytes), len(data)))
            f.write(name_bytes)
            f.write(data)
        f.write(struct.pack("<II", 0, 0))

    print(f"apps_blob: {len(names)} binaries -> {os.path.basename(outfile)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())