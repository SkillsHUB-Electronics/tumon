"""Builds the app icon from the Tumon logo.

    python tools/make_icon.py [source-image]

Reads the full logo (assets/logo/tumon-logo.png by default; any format Pillow reads), cuts out the emblem, the
rounded tile with the T and the camera (without the word "tumon", which is unreadable at icon size), makes the
white background around it transparent (the white "T" inside stays), and writes:
    assets/logo/tumon-icon.png   256 x 256, transparent
    cpp/app/tumon.ico            16 to 256 pixels, used by Tumon.exe and the installer
Needs Pillow. Run it again only when the logo changes.
"""
from __future__ import annotations

import sys
from collections import deque
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LOGO = ROOT / "assets" / "logo" / "tumon-logo.png"
ICON_PNG = ROOT / "assets" / "logo" / "tumon-icon.png"
ICO = ROOT / "cpp" / "app" / "tumon.ico"

BRAND_BLUE = (0, 45, 105)   # the tile's edge colour, used to clean the anti-aliased edge
TEXT_STARTS = 0.67          # the word "tumon" begins below this fraction of the height
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def cut_background(image: Image.Image) -> Image.Image:
    """Transparent background: flood fill near-white from the border, then soften the edge."""
    rgb = image.convert("RGB")
    w, h = rgb.size
    px = rgb.load()
    background = bytearray(w * h)
    queue: deque[tuple[int, int]] = deque()

    def visit(x: int, y: int) -> None:
        if not background[y * w + x] and min(px[x, y]) >= 235:
            background[y * w + x] = 1
            queue.append((x, y))

    for x in range(w):
        visit(x, 0)
        visit(x, h - 1)
    for y in range(h):
        visit(0, y)
        visit(w - 1, y)
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h:
                visit(nx, ny)

    out = rgb.convert("RGBA")
    op = out.load()
    for y in range(h):
        for x in range(w):
            if background[y * w + x]:
                op[x, y] = (*BRAND_BLUE, 0)
                continue
            near = any(
                0 <= x + dx < w and 0 <= y + dy < h and background[(y + dy) * w + x + dx]
                for dx in (-2, 0, 2) for dy in (-2, 0, 2)
            )
            if near:  # a blend of the blue outline and white: recover how much is blue from the red channel
                alpha = max(0, min(255, 255 - px[x, y][0]))
                op[x, y] = (*BRAND_BLUE, alpha)
    return out


def square_emblem(transparent: Image.Image) -> Image.Image:
    w, h = transparent.size
    emblem = transparent.crop((0, 0, w, int(h * TEXT_STARTS)))
    box = emblem.getchannel("A").point(lambda a: 255 if a > 16 else 0).getbbox()
    if not box:
        raise SystemExit("No emblem found in the image.")
    emblem = emblem.crop(box)
    side = int(max(emblem.size) * 1.04)
    canvas = Image.new("RGBA", (side, side), (*BRAND_BLUE, 0))
    canvas.paste(emblem, ((side - emblem.width) // 2, (side - emblem.height) // 2))
    return canvas


def main() -> None:
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else LOGO
    image = Image.open(source)
    image.load()
    LOGO.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() != LOGO.resolve():
        image.convert("RGB").save(LOGO, optimize=True)
    emblem = square_emblem(cut_background(image))
    big = emblem.resize((256, 256), Image.LANCZOS)
    big.save(ICON_PNG, optimize=True)
    frames = [emblem.resize((s, s), Image.LANCZOS) for s in SIZES]
    frames[-1].save(ICO, format="ICO", sizes=[(s, s) for s in SIZES], append_images=frames[:-1], bitmap_format="bmp")
    print(f"wrote {LOGO}\nwrote {ICON_PNG}\nwrote {ICO}")


if __name__ == "__main__":
    main()
