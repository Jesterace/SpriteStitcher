#!/usr/bin/env python3
import struct
import zlib
from pathlib import Path

width = 24
height = 24

# Simple RGB colors chosen to stress symbol readability.
colors = [
    (252, 251, 248),  # near white
    (10, 10, 10),     # near black
    (227, 29, 66),    # red
    (71, 167, 47),    # green
    (19, 71, 125),    # dark blue
    (255, 203, 0),    # yellow
    (156, 36, 98),    # plum
    (48, 194, 236),   # bright blue
]

rows = []
for y in range(height):
    row = bytearray()
    row.append(0)  # PNG filter type 0
    for x in range(width):
        block_x = x // 6
        block_y = y // 6
        idx = (block_y * 4 + block_x) % len(colors)

        # Add a tiny checker/stripe variation so repeated colors are obvious.
        if (x + y) % 11 == 0:
            idx = (idx + 1) % len(colors)

        row.extend(colors[idx])
    rows.append(bytes(row))

raw = b"".join(rows)

def chunk(kind, data):
    payload = kind + data
    return (
        struct.pack(">I", len(data)) +
        payload +
        struct.pack(">I", zlib.crc32(payload) & 0xffffffff)
    )

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw, 9))
png += chunk(b"IEND", b"")

out = Path("samples/pdf_qa_sprite.png")
out.write_bytes(png)
print(out)
