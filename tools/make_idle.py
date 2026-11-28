"""Prepares the picture Tumon Camera shows while no program is sending a camera.

    python tools/make_idle.py

Reads assets/idle/tumon-idle.png (16:9 artwork with a spinner drawn in it) and writes cpp/src/source/idle.png,
1280 x 720, with the drawn spinner painted out: the camera draws a turning one in the same place, so the receiver
can see the video is alive. Needs Pillow. Run again only when the artwork changes; if the spinner moves in new
artwork, update SPINNER below and the matching constants in cpp/src/source/idle_screen.cpp.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "idle" / "tumon-idle.png"
OUT = ROOT / "cpp" / "src" / "source" / "idle.png"

# Spinner in the source artwork, as fractions of its size: centre x, centre y, and the radius to paint out
# (a little more than the ring's own radius) as a fraction of the height.
SPINNER = (0.5, 0.7837, 0.064)


def erase_spinner(image: Image.Image) -> None:
    """Replaces a disc with a left-to-right blend of the background on both sides of it (a smooth gradient)."""
    w, h = image.size
    cx, cy, radius = SPINNER[0] * w, SPINNER[1] * h, SPINNER[2] * h
    px = image.load()
    for y in range(int(cy - radius), int(cy + radius) + 1):
        half = math.sqrt(max(0.0, radius * radius - (y - cy) ** 2))
        left, right = int(cx - half) - 1, int(cx + half) + 1
        a, b = px[left, y], px[right, y]
        for x in range(left + 1, right):
            t = (x - left) / (right - left)
            px[x, y] = tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def main() -> None:
    image = Image.open(SOURCE).convert("RGB")
    erase_spinner(image)
    image.resize((1280, 720), Image.LANCZOS).save(OUT, optimize=True)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
