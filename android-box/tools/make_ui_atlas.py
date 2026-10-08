#!/usr/bin/env python3
"""Draws the side-bar button icons into app/src/main/assets/ui_atlas.rgba
(u32 width, u32 height, then RGBA rows top to bottom). Icon i is the 128x128 cell at x = 128*i.
Order must match the ICON_* list in app/src/main/cpp/overlay.h."""
import struct
import sys
from PIL import Image, ImageDraw, ImageFont

S = 128
ICONS = ["RIGHT", "CROUCH", "STAND", "VIEW", "ALL", "MENU", "SAVE", "LOAD", "PAUSE", "MAP", "SWAP"]
INK = (244, 228, 190, 255)
FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 21)


def figure(d, x, y, crouch=False, scale=1.0):
    w = max(3, int(7 * scale))
    r = 9 * scale
    if crouch:
        d.ellipse((x - r + 10 * scale, y - r + 22 * scale, x + r + 10 * scale, y + r + 22 * scale), fill=INK)
        d.line((x + 8 * scale, y + 34 * scale, x - 10 * scale, y + 48 * scale), fill=INK, width=w)  # back
        d.line((x - 10 * scale, y + 48 * scale, x + 10 * scale, y + 58 * scale), fill=INK, width=w)  # thigh
        d.line((x + 10 * scale, y + 58 * scale, x + 2 * scale, y + 74 * scale), fill=INK, width=w)  # shin
        d.line((x + 6 * scale, y + 40 * scale, x + 22 * scale, y + 52 * scale), fill=INK, width=w)  # arm
    else:
        d.ellipse((x - r, y - r + 6 * scale, x + r, y + r + 6 * scale), fill=INK)
        d.line((x, y + 16 * scale, x, y + 48 * scale), fill=INK, width=w)
        d.line((x, y + 48 * scale, x - 10 * scale, y + 74 * scale), fill=INK, width=w)
        d.line((x, y + 48 * scale, x + 10 * scale, y + 74 * scale), fill=INK, width=w)
        d.line((x, y + 24 * scale, x - 14 * scale, y + 42 * scale), fill=INK, width=w)
        d.line((x, y + 24 * scale, x + 14 * scale, y + 42 * scale), fill=INK, width=w)


def draw(name, d):
    c = S // 2
    if name == "RIGHT":  # mouse with the right button lit
        d.rounded_rectangle((c - 22, 14, c + 22, 84), radius=20, outline=INK, width=6)
        d.line((c - 22, 42, c + 22, 42), fill=INK, width=5)
        d.line((c, 14, c, 42), fill=INK, width=5)
        # the right button: the mouse body's top-right quarter, filled
        body = Image.new("L", (S, S), 0)
        ImageDraw.Draw(body).rounded_rectangle((c - 22, 14, c + 22, 84), radius=20, fill=255)
        quarter = Image.new("L", (S, S), 0)
        ImageDraw.Draw(quarter).rectangle((c, 14, c + 22, 42), fill=255)
        from PIL import ImageChops
        d._image.paste(INK, (0, 0), ImageChops.multiply(body, quarter))
    elif name == "CROUCH":
        figure(d, c - 6, 8, crouch=True)
    elif name == "STAND":
        figure(d, c, 6)
    elif name == "VIEW":  # an eye with a viewing cone
        d.polygon([(c - 34, 50), (c + 40, 18), (c + 40, 82)], outline=INK, width=4)
        d.ellipse((c - 46, 36, c - 14, 64), outline=INK, width=5)
        d.ellipse((c - 36, 44, c - 24, 56), fill=INK)
    elif name == "ALL":
        for k, dx in enumerate((-30, 0, 30)):
            figure(d, c + dx, 14 + (8 if k != 1 else 0), scale=0.75)
    elif name == "MENU":
        for k in range(3):
            d.rounded_rectangle((c - 32, 22 + k * 22, c + 32, 34 + k * 22), radius=5, fill=INK)
    elif name == "SAVE":
        d.line((c, 14, c, 58), fill=INK, width=9)
        d.polygon([(c - 22, 46), (c + 22, 46), (c, 70)], fill=INK)
        d.line((c - 34, 66, c - 34, 84, c + 34, 84, c + 34, 66), fill=INK, width=7)
    elif name == "LOAD":
        d.line((c, 30, c, 72), fill=INK, width=9)
        d.polygon([(c - 22, 36), (c + 22, 36), (c, 12)], fill=INK)
        d.line((c - 34, 66, c - 34, 84, c + 34, 84, c + 34, 66), fill=INK, width=7)
    elif name == "PAUSE":
        d.rectangle((c - 24, 18, c - 8, 80), fill=INK)
        d.rectangle((c + 8, 18, c + 24, 80), fill=INK)
    elif name == "MAP":
        pts = [(c - 40, 24), (c - 14, 14), (c + 14, 24), (c + 40, 14), (c + 40, 76), (c + 14, 86), (c - 14, 76), (c - 40, 86)]
        d.polygon(pts, outline=INK, width=5)
        d.line((c - 14, 14, c - 14, 76), fill=INK, width=4)
        d.line((c + 14, 24, c + 14, 86), fill=INK, width=4)
    elif name == "SWAP":  # two arrows going opposite ways
        d.line((c - 30, 34, c + 22, 34), fill=INK, width=8)
        d.polygon([(c + 18, 20), (c + 38, 34), (c + 18, 48)], fill=INK)
        d.line((c - 22, 66, c + 30, 66), fill=INK, width=8)
        d.polygon([(c - 18, 52), (c - 38, 66), (c - 18, 80)], fill=INK)
    w = d.textlength(name, font=FONT)
    d.text((c - w / 2, 96), name, font=FONT, fill=INK)


def main(out):
    atlas = Image.new("RGBA", (S * len(ICONS), S), (0, 0, 0, 0))
    for i, name in enumerate(ICONS):
        cell = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        draw(name, ImageDraw.Draw(cell))
        atlas.paste(cell, (i * S, 0))
    with open(out, "wb") as f:
        f.write(struct.pack("<II", atlas.width, atlas.height))
        f.write(atlas.tobytes())
    atlas.save(out + ".preview.png")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "app/src/main/assets/ui_atlas.rgba")
