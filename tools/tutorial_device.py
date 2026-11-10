#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Device graphics and the README banner for the Caravel tutorials.

The FM-1 front is drawn from the emulator's own wide layout (web/emu/index.html LAYOUTS.wide: the same positions of the screen, the eight
knobs, the OCT pair, the 6 x 2 button grid and the 27 keys), flat and stylised, not a photo. A control can be highlighted and labelled; the
screen shows a real firmware frame (docs/tutorials/img/*.png from tools/start_tutorial_frames.py) when one is given. Every SVG is self-contained
(no external font, image or script) and uses its own dark body, so it reads the same on a light or a dark page.

  tools/tutorial_media_docker.sh tutorial_device.py        writes docs/tutorials/img/device-*.svg and tutorials-banner.png

The banner (Pillow, Inter Tight from assets/fonts, OFL-1.1) is the picture at the top of the README that links to the tutorials.
"""
import base64
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / "docs/tutorials/img"
sys.path.insert(0, str(ROOT / "web"))

# ---------------------------------------------------------------------------------------------- the front (emulator's wide layout)
W, H = 1000, 624
SCREEN = (292, 44, 256)
KNOBS = {"MASTER": (105, 100, 24), "SELECT": (215, 100, 24), "PRESETS": (105, 205, 24), "ALGORITHM": (215, 205, 24),
         "KNOB 1": (610, 100, 22), "KNOB 2": (714, 100, 22), "KNOB 3": (818, 100, 22), "KNOB 4": (922, 100, 22)}
ROW1 = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO"]
ROW2 = ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
GRID = (582, 174, 54, 48, 63, 62)
OCT = {"OCT-": (60, 258, 90, 34), "OCT+": (170, 258, 90, 34)}
TRAYS = [(50, 250, 220, 50), (568, 160, 400, 140)]
KEYBED = (36, 332, 928, 256)
WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
PITCH = (KEYBED[2] - 24) / 16
KW, KH = PITCH * 0.78, KEYBED[3] * 0.425

CYAN, AMBER, GREEN, VIOLET, SALMON = "#63d7e6", "#ffaa29", "#8fd694", "#c9a0f0", "#f28b82"
INK = "#d6d6d8"
FONT = "Inter Tight, -apple-system, 'Helvetica Neue', Arial, sans-serif"


def key_box(k):
    """The box of physical key k (0 = F3 .. 26 = G5)."""
    x0, y0, w, h = KEYBED
    if k in WHITE:
        cx, cy = x0 + 12 + (WHITE.index(k) + 0.5) * PITCH, y0 + h * 0.75
    else:
        cx, cy = x0 + 12 + (BLACK_AFTER[BLACK.index(k)] + 1) * PITCH, y0 + h * 0.25
    return cx - KW / 2, cy - KH / 2, KW, KH


def box(name):
    """(x, y, w, h) of a named control; KEYS is the whole key bed, KEY n a single key, ROW1/ROW2 a button row."""
    if name in KNOBS:
        x, y, r = KNOBS[name]
        return x - r - 8, y - r - 8, 2 * r + 16, 2 * r + 16
    if name in OCT:
        return OCT[name]
    gx, gy, gw, gh, dx, dy = GRID
    for r, row in enumerate((ROW1, ROW2)):
        if name in row:
            return gx + row.index(name) * dx, gy + r * dy, gw, gh
    if name == "KEYS":
        return KEYBED
    if name == "SCREEN":
        return SCREEN[0], SCREEN[1], SCREEN[2], SCREEN[2]
    if name == "PAGES":                      # the ten page buttons (everything in the grid but PLAY and REC)
        return gx, gy, 6 * dx - (dx - gw), dy + gh
    if name == "KNOB 1-4":
        return 610 - 30, 100 - 30, 922 - 610 + 60, 60
    if name == "OCT":
        return 60, 258, 200, 34
    if name == "TRANSPORT":
        return gx + 4 * dx, gy + dy, 2 * dx - (dx - gw), gh
    if name.startswith("KEY "):
        return key_box(int(name[4:]))
    raise KeyError(name)


def esc(s):
    return html.escape(s, quote=True)


SECRET_LIKE = re.compile(r"(?:AKIA|ASIA)[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{20,}|sk-[A-Za-z0-9_-]{20,}|xox[baprs]-[A-Za-z0-9-]{20,}", re.IGNORECASE)


def png_b64(path):
    """The PNG as base64 for a data URI. A random run of base64 can look like a key to the publication check (AKIA/ASIA + 16 characters);
    then a small tEXt chunk after IHDR changes the bytes, deterministically, until no such run is left."""
    import struct
    import zlib
    raw = Path(path).read_bytes()
    for pad in range(0, 64):
        data = raw
        if pad:
            body = b"Comment\x00caravel tutorial frame " + b"." * pad
            chunk = struct.pack(">I", len(body)) + b"tEXt" + body + struct.pack(">I", zlib.crc32(b"tEXt" + body) & 0xFFFFFFFF)
            data = raw[:33] + chunk + raw[33:]                      # 8 signature bytes + the 25 bytes of IHDR
        text = base64.b64encode(data).decode("ascii")
        if not SECRET_LIKE.search(text):
            return text
    raise SystemExit("could not avoid a key-like run in " + str(path))


def device_svg(highlight=(), screen=None, title="The FM-1 front", desc="", colour=CYAN):
    """highlight: [(control, colour, legend text or None)] ringed on the front; the ones with a text get a numbered badge and a legend line
    under the picture. screen: a 240 x 240 PNG shown in the display. The picture sits on its own dark card, so it reads on any page."""
    PAD = 34
    legend = [(i, h) for i, h in enumerate([h for h in highlight if not isinstance(h, str) and len(h) > 2 and h[2]], 1)]
    FS, LH = 40, 70
    rows, row, x = [], [], 0
    for n, h in legend:                                        # lay the legend out in rows no wider than the front
        wd = 66 + len(h[2]) * FS * 0.6 + 40
        if row and x + wd > W - 20:
            rows.append(row); row, x = [], 0
        row.append((n, h, x, wd)); x += wd
    if row:
        rows.append(row)
    mb = (len(rows) * LH + 30) if rows else 0
    vx, vy, vw, vh = -PAD, -PAD, W + 2 * PAD, H + 2 * PAD + mb
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vx} {vy} {vw} {vh}" role="img" aria-labelledby="t d">',
           f'<title id="t">{esc(title)}</title><desc id="d">{esc(desc or title)}</desc>',
           '<defs><linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#232326"/><stop offset="1" stop-color="#18181a"/></linearGradient>'
           '<linearGradient id="cap" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#303034"/><stop offset="1" stop-color="#26262a"/></linearGradient></defs>',
           f'<rect x="{vx}" y="{vy}" width="{vw}" height="{vh}" rx="26" fill="#100c08"/>',
           f'<rect x="4" y="4" width="{W - 8}" height="{H - 8}" rx="40" fill="url(#body)" stroke="#34343a" stroke-width="2"/>']
    for x, y, w, h in TRAYS:
        out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="14" fill="#111112"/>')
    sx, sy, ss = SCREEN
    out.append(f'<rect x="{sx}" y="{sy}" width="{ss}" height="{ss}" rx="18" fill="#0b0b0c" stroke="#2a2a2e" stroke-width="2"/>')
    inset = ss / 32
    if screen:
        out.append(f'<image x="{sx + inset}" y="{sy + inset}" width="{ss - 2 * inset}" height="{ss - 2 * inset}" preserveAspectRatio="none" '
                   f'style="image-rendering:pixelated" image-rendering="pixelated" href="data:image/png;base64,{png_b64(screen)}"/>')
    else:
        out.append(f'<rect x="{sx + inset}" y="{sy + inset}" width="{ss - 2 * inset}" height="{ss - 2 * inset}" rx="6" fill="#16120a"/>')
    for name, (x, y, r) in KNOBS.items():
        out.append(f'<circle cx="{x}" cy="{y}" r="{r + 5}" fill="#111112"/><circle cx="{x}" cy="{y}" r="{r}" fill="url(#cap)" stroke="#3c3c42" stroke-width="2"/>')
        out.append(f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 14}" stroke="{INK}" stroke-width="3" stroke-linecap="round"/>')
        out.append(f'<text x="{x}" y="{y - r - 12}" text-anchor="middle" font-family="{FONT}" font-size="14" font-weight="600" fill="#9a9aa0" letter-spacing=".6">{esc(name)}</text>')

    def button(name, sub=""):
        x, y, w, h = box(name)
        ink = "#ff6b61" if name == "REC" else INK
        out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="url(#cap)" stroke="#3c3c42" stroke-width="1.5"/>')
        if sub:
            out.append(f'<text x="{x + w / 2}" y="{y + h / 2 - 1}" text-anchor="middle" font-family="{FONT}" font-size="12" font-weight="700" fill="{ink}">{esc(name)}</text>'
                       f'<text x="{x + w / 2}" y="{y + h / 2 + 13}" text-anchor="middle" font-family="{FONT}" font-size="11" font-weight="600" fill="#9a9aa0">{esc(sub)}</text>')
        else:
            out.append(f'<text x="{x + w / 2}" y="{y + h / 2 + 5}" text-anchor="middle" font-family="{FONT}" font-size="{12 if name in OCT else 13}" font-weight="700" fill="{ink}">{esc(name.replace("-", "−"))}</text>')
    for n in OCT:
        button(n)
    for n in ROW1 + ROW2:
        button(n, "STOP" if n == "PLAY" else "")
    out.append(f'<rect x="{KEYBED[0]}" y="{KEYBED[1]}" width="{KEYBED[2]}" height="{KEYBED[3]}" rx="18" fill="#111112"/>')
    for k in range(27):
        x, y, w, h = key_box(k)
        sw = max(3.0, w * 0.14)
        out.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{w / 2:.1f}" fill="#0c0c0d" stroke="#3a3a3e" stroke-width="1.5"/>'
                   f'<rect x="{x + w / 2 - sw / 2:.1f}" y="{y + w * 0.4:.1f}" width="{sw:.1f}" height="{h - w * 0.8:.1f}" rx="{sw / 2:.1f}" fill="#3a3a3c"/>')
    # highlights: a soft halo and a solid ring in the colour of the group, a numbered badge when the control is in the legend
    n_badge = 0
    for item in highlight:
        name, col = (item, colour) if isinstance(item, str) else item[:2]
        text = None if isinstance(item, str) or len(item) < 3 else item[2]
        x, y, w, h = box(name)
        rx = (w / 2 if name in KNOBS else 12)
        pad = 7
        if name in KNOBS:
            cx, cy = x + w / 2, y + h / 2
            out.append(f'<circle cx="{cx}" cy="{cy}" r="{w / 2 + 6}" fill="{col}" fill-opacity=".16" stroke="{col}" stroke-opacity=".35" stroke-width="10"/>'
                       f'<circle cx="{cx}" cy="{cy}" r="{w / 2}" fill="none" stroke="{col}" stroke-width="4"/>')
            bx, by = cx + w / 2 - 4, cy - w / 2 + 4
        else:
            out.append(f'<rect x="{x - pad}" y="{y - pad}" width="{w + 2 * pad}" height="{h + 2 * pad}" rx="{rx}" fill="{col}" fill-opacity=".16" stroke="{col}" stroke-opacity=".35" stroke-width="10"/>'
                       f'<rect x="{x - pad}" y="{y - pad}" width="{w + 2 * pad}" height="{h + 2 * pad}" rx="{rx}" fill="none" stroke="{col}" stroke-width="4"/>')
            bx, by = (x + w / 2, y - pad) if w < 120 else (x - pad + 4, y - pad + 4)   # small buttons: centred above, so neighbours' badges never touch
        if text:
            n_badge += 1
            bx = min(max(bx, 16), W - 16)
            out.append(f'<circle cx="{bx:.1f}" cy="{by:.1f}" r="23" fill="{col}" stroke="#100c08" stroke-width="3"/>'
                       f'<text x="{bx:.1f}" y="{by + 12:.1f}" text-anchor="middle" font-family="{FONT}" font-size="34" font-weight="800" fill="#100c08">{n_badge}</text>')
    for r, row in enumerate(rows):                             # the legend under the front
        y0 = H + 12 + r * LH
        total = row[-1][2] + row[-1][3]
        for n, h, x0, wd in row:
            x0 += (W - total) / 2
            out.append(f'<circle cx="{x0 + 25:.1f}" cy="{y0 + 28}" r="25" fill="{h[1]}"/>'
                       f'<text x="{x0 + 25:.1f}" y="{y0 + 41}" text-anchor="middle" font-family="{FONT}" font-size="36" font-weight="800" fill="#100c08">{n}</text>'
                       f'<text x="{x0 + 62:.1f}" y="{y0 + 41}" font-family="{FONT}" font-size="{FS}" font-weight="700" fill="#f7efde">{esc(h[2])}</text>')
    out.append("</svg>")
    return "\n".join(out)


# ---------------------------------------------------------------------------------------------- the figures the tutorials use
SPECS = {
    "device-hero": dict(screen="start-home.png", title="The M-VAVE FM-1 with the Caravel HOME screen",
                        desc="A stylised drawing of the FM-1 front with a real firmware frame on its display."),
    "device-controls": dict(
        screen="start-home.png", title="The FM-1 controls", desc="The knobs, the page buttons, the transport, the octave buttons and the keys.",
        highlight=[("ALGORITHM", CYAN, "TRACK"), ("PRESETS", CYAN, "SOUND"), ("KNOB 1-4", AMBER, "KNOB 1-4"), ("PAGES", CYAN, "PAGE BUTTONS"),
                   ("TRANSPORT", GREEN, "PLAY · REC"), ("OCT", VIOLET, "OCTAVE"), ("KEYS", SALMON, "KEYS")]),
    "device-first-sound": dict(
        screen="start-home.png", title="Play and change a sound", desc="ALGORITHM chooses the track, PRESETS the sound, KNOB 1 to 4 edit the four cards, the keys play.",
        highlight=[("ALGORITHM", CYAN, "TRACK"), ("PRESETS", CYAN, "SOUND"), ("KNOB 1-4", AMBER, "KNOB 1-4"), ("KEYS", SALMON, "PLAY")]),
    "device-engine": dict(
        screen="start-engine-picker.png", title="Choose an engine", desc="Hold EDIT, then press a white key.",
        highlight=[("EDIT", CYAN, "HOLD EDIT"), ("KEYS", SALMON, "WHITE KEY")]),
    "device-step": dict(
        screen="start-step-notes.png", title="Enter a sequence", desc="Press SEQ, arm REC and play keys; each key writes a note and moves one step on.",
        highlight=[("SEQ", CYAN, "SEQ"), ("REC", "#ff6b61", "REC"), ("KEYS", SALMON, "NOTES")]),
    "device-play": dict(
        screen="start-step-play.png", title="Play the sequence", desc="Switch REC off and press PLAY.",
        highlight=[("REC", "#ff6b61", "REC OFF"), ("PLAY", GREEN, "PLAY")]),
    "device-action": dict(
        screen="harmony-bank.png", title="The ACTION keys", desc="With ACTION on, C#5 and D#5 step through the banks and D5, E5, F5, G5 are A1 to A4.",
        highlight=[("KEY 20", VIOLET, "BANK BACK"), ("KEY 22", VIOLET, "BANK NEXT"), ("KEY 21", AMBER, "A1"), ("KEY 23", AMBER, "A2"), ("KEY 24", AMBER, "A3"), ("KEY 26", AMBER, "A4")]),
    "device-save": dict(
        screen="start-save-arm.png", title="Save a project", desc="SAVE four times to PROJECT, KNOB 1 picks the slot, KNOB 4 SAVE, OCT+ twice.",
        highlight=[("SAVE", CYAN, "SAVE ×4"), ("KNOB 1", AMBER, "SLOT"), ("KNOB 4", AMBER, "SAVE"), ("OCT+", VIOLET, "OCT+ ×2")]),
}


def banner():
    """tutorials-banner.png: the picture at the top of the README that links to the tutorials page."""
    from PIL import Image, ImageDraw, ImageFont
    import make_site
    S = 2
    w, h = 1600, 440
    bg0, bg1 = (16, 12, 8), (34, 25, 12)
    im = Image.new("RGB", (w, h), bg0)
    px = im.load()
    for y in range(h):                                           # a soft vertical glow
        for x in range(0, w):
            t = y / h * 0.55 + x / w * 0.45
            px[x, y] = tuple(int(bg0[i] + (bg1[i] - bg0[i]) * t) for i in range(3))
    dr = ImageDraw.Draw(im)
    font_file = ROOT / "assets/fonts/InterTight[wght].ttf"

    def font(size, weight):
        f = ImageFont.truetype(str(font_file), size)
        try:
            f.set_variation_by_axes([weight])
        except Exception:
            pass
        return f
    amber, cyan, text, dim = (255, 170, 41), (99, 215, 230), (247, 239, 222), (176, 160, 128)
    dr.rounded_rectangle((6, 6, w - 7, h - 7), radius=26, outline=(84, 65, 31), width=3)
    # the pixel ship (the firmware's own bitmap, as the site header draws it)
    unit = 5
    x0, y0 = 70, 62
    for part in make_site.SHIP_PATH.split("M")[1:]:
        m = re.fullmatch(r"(\d+)(?:,| )?(\d+)h(\d+)v(\d+)h-\d+z", part)
        if m:
            a, b, c, d = map(int, m.groups())
            dr.rectangle((x0 + a * unit, y0 + b * unit, x0 + (a + c) * unit - 1, y0 + (b + d) * unit - 1), fill=amber)
    dr.text((70 + 28 * unit + 34, 56), "CARAVEL", font=font(48, 800), fill=amber)
    dr.text((70 + 28 * unit + 34, 108), "TUTORIALS", font=font(48, 800), fill=text)
    dr.text((70, 196), "Learn the FM-1 step by step", font=font(52, 700), fill=text)
    dr.text((70, 268), "Start here  \u00b7  Engines  \u00b7  Sequencer  \u00b7  Perform  \u00b7  Harmony", font=font(27, 500), fill=dim)
    # the call to action
    label = "OPEN THE TUTORIALS  ›"
    f = font(32, 750)
    tw = int(dr.textlength(label, font=f))
    bx0, by0 = 70, 332
    dr.rounded_rectangle((bx0, by0, bx0 + tw + 64, by0 + 66), radius=33, fill=amber)
    dr.text((bx0 + 32, by0 + 12), label, font=f, fill=(26, 18, 5))
    # four real firmware frames in their bezels
    shots = ["card-start.png", "card-sem.png", "card-harmony.png"]
    size, gap = 204, 26
    total = len(shots) * size + (len(shots) - 1) * gap
    sx = w - 70 - total
    for i, name in enumerate(shots):
        x = sx + i * (size + gap)
        y = 104 + (28 if i % 2 else 0)
        dr.rounded_rectangle((x - 10, y - 10, x + size + 10, y + size + 10), radius=18, fill=(11, 11, 12), outline=(60, 60, 66), width=3)
        dr.rounded_rectangle((x - 10, y - 10, x + size + 10, y + size + 10), radius=18, outline=cyan if i == 0 else (60, 60, 66), width=3)
        fr = Image.open(IMG / name).convert("RGB").resize((size, size), Image.NEAREST)
        im.paste(fr, (x, y))
    out = IMG / "tutorials-banner.png"
    im.save(out, optimize=True)
    return out


def main():
    for name, spec in SPECS.items():
        svg = device_svg(highlight=spec.get("highlight", ()),
                         screen=IMG / spec["screen"] if spec.get("screen") else None, title=spec["title"], desc=spec.get("desc", ""))
        (IMG / (name + ".svg")).write_text(svg, encoding="utf-8")
        print("wrote", name + ".svg", len(svg), "bytes")
    if "--no-banner" not in sys.argv:
        print("wrote", banner())


if __name__ == "__main__":
    main()
