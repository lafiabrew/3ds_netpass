#!/usr/bin/env python3
"""
Deterministic generator for StreetPass & Coin Vault (3ds_netpass) packaging assets:
- meta/icon.png (48x48 RGBA PNG: Golden 3DS Play Coin + StreetPass Neon Green Beacon)
- meta/banner.png (256x128 RGBA PNG: Play Coin & StreetPass Beacon + title text)
- meta/audio.wav & meta/banner.bnr
"""

import math
import os
import struct
import wave
import zlib
from generate_bnr_shared import draw_banner_text_centered, write_cbmd_bnr  # type: ignore


def write_png(path: str, width: int, height: int, rgba_bytes: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw_rows = bytearray()
    stride = width * 4
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(raw_rows), 9)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))


def generate_icon(path: str) -> None:
    w, h = 48, 48
    buf = bytearray(w * h * 4)
    cx, cy = 22.0, 25.0
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            if x < 2 or x >= w - 2 or y < 2 or y >= h - 2:
                buf[idx : idx + 4] = bytes((80, 250, 125, 255))
                continue
            buf[idx : idx + 4] = bytes((12, 26, 18, 255))

            # StreetPass green glowing orb in top-right (36, 12)
            if (x - 36) ** 2 + (y - 12) ** 2 <= 28:
                buf[idx : idx + 4] = bytes((75, 255, 120, 255))
                continue

            # Golden Play Coin (center 22, 25, radius 14)
            r2 = (x - cx) ** 2 + (y - cy) ** 2
            if r2 <= 14.0 * 14.0:
                if r2 >= 11.5 * 11.5:
                    buf[idx : idx + 4] = bytes((235, 175, 45, 255))
                elif abs(x - cx) <= 2.2 and abs(y - cy) <= 6.5:
                    # Vertical coin slot embossing
                    buf[idx : idx + 4] = bytes((255, 248, 175, 255))
                else:
                    buf[idx : idx + 4] = bytes((255, 215, 70, 255))

    write_png(path, w, h, bytes(buf))


def generate_banner(path: str) -> bytes:
    w, h = 256, 128
    buf = bytearray(w * h * 4)
    cx, cy = 128.0, 44.0
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            r = 10 + (y * 12) // h
            g = 28 + (y * 28) // h
            b = 18 + (y * 20) // h
            buf[idx : idx + 4] = bytes((r, g, b, 255))

            r2 = (x - cx) ** 2 + (y - cy) ** 2
            if r2 <= 28.0 * 28.0:
                if r2 >= 23.0 * 23.0:
                    buf[idx : idx + 4] = bytes((235, 175, 45, 255))
                elif abs(x - cx) <= 4.0 and abs(y - cy) <= 13.0:
                    buf[idx : idx + 4] = bytes((255, 250, 185, 255))
                else:
                    buf[idx : idx + 4] = bytes((255, 215, 70, 255))

    draw_banner_text_centered(buf, w, h, 86, "NETPASS", 2, (110, 255, 145, 255))
    draw_banner_text_centered(buf, w, h, 108, "PLAY COINS & STREETPASS", 1, (255, 235, 140, 255))
    raw = bytes(buf)
    write_png(path, w, h, raw)
    return raw


def generate_audio(path: str) -> None:
    sample_rate = 22050
    duration_s = 0.8
    n_samples = int(sample_rate * duration_s)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            t = i / sample_rate
            freq = 523.25 if t < 0.2 else (783.99 if t < 0.45 else 1046.50)
            env = max(0.0, 1.0 - (t / duration_s))
            val = int(12000 * env * math.sin(2.0 * math.pi * freq * t))
            frames.extend(struct.pack("<h", val))
        wf.writeframes(bytes(frames))


def main() -> None:
    meta_dir = os.path.dirname(os.path.abspath(__file__))
    generate_icon(os.path.join(meta_dir, "icon.png"))
    banner_rgba = generate_banner(os.path.join(meta_dir, "banner.png"))
    generate_audio(os.path.join(meta_dir, "audio.wav"))
    write_cbmd_bnr(os.path.join(meta_dir, "banner.bnr"), (523.25, 783.99, 1046.50), banner_rgba)
    print("Generated NetPass meta/icon.png, meta/banner.png, meta/audio.wav, meta/banner.bnr")


if __name__ == "__main__":
    main()
