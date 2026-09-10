#!/usr/bin/env python3
"""Render README preview posters from the checked-in Pixel Clock layout and art."""
from __future__ import annotations

from calendar import monthrange
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/assets/cat-focus-calendar/publish-3x4"
ASSETS = ROOT / "assets/images/pixel-clock"

PAPER = "#F5F0E3"
INK = "#17263A"
GREEN = "#506A4D"
GREEN_DARK = "#304B38"
RUST = "#B65B3F"
MUTED = "#7C7A70"
SHADOW = "#D8CDB6"
SUN = "#EFD8A4"

DIGITS = (
    (0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E),
    (0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E),
    (0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F),
    (0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E),
    (0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02),
    (0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E),
    (0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E),
    (0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08),
    (0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E),
    (0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C),
)

FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
MONO = Path("/System/Library/Fonts/Supplemental/Andale Mono.ttf")


def font(size: int, *, mono: bool = False) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(MONO if mono else FONT), size=size)


def center(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], value: str,
           fill: str, style: ImageFont.FreeTypeFont) -> None:
    left, top, right, bottom = box
    anchor = "mm"
    draw.text(((left + right) // 2, (top + bottom) // 2), value, fill=fill,
              font=style, anchor=anchor)


def rect(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, fill: str) -> None:
    draw.rectangle((x, y, x + w - 1, y + h - 1), fill=fill)


def digit(draw: ImageDraw.ImageDraw, x: int, y: int, scale: int, value: int,
          fill: str = INK) -> None:
    for row, bits in enumerate(DIGITS[value]):
        for col in range(5):
            if bits & (1 << (4 - col)):
                rect(draw, x + col * scale, y + row * scale, scale, scale, fill)


def digits(draw: ImageDraw.ImageDraw, x: int, y: int, scale: int, value: str,
           spacing: int = 2, fill: str = INK) -> None:
    advance = 5 * scale + spacing
    for i, char in enumerate(value):
        digit(draw, x + i * advance, y, scale, int(char), fill)


def colon(draw: ImageDraw.ImageDraw, x: int, y: int, scale: int, fill: str = INK) -> None:
    rect(draw, x, y + 2 * scale, scale, scale, fill)
    rect(draw, x, y + 4 * scale, scale, scale, fill)


def status_icons(draw: ImageDraw.ImageDraw, *, connected: bool = True) -> None:
    wifi = GREEN if connected else INK
    rect(draw, 174, 12, 4, 4, wifi)
    rect(draw, 180, 9, 4, 4, wifi)
    rect(draw, 186, 12, 4, 4, wifi)
    rect(draw, 180, 16, 4, 4, wifi)
    rect(draw, 207, 10, 28, 14, INK)
    rect(draw, 211, 13, 20, 8, PAPER)
    rect(draw, 235, 14, 3, 6, INK)
    for x in (213, 219, 225):
        rect(draw, x, 15, 4, 4, GREEN)


def nav_icon(draw: ImageDraw.ImageDraw, x: int, kind: str, y: int = 286) -> None:
    if kind == "up":
        rect(draw, x + 30, y + 8, 12, 4, PAPER)
        rect(draw, x + 33, y + 5, 6, 4, PAPER)
        rect(draw, x + 35, y + 2, 2, 4, PAPER)
        rect(draw, x + 33, y + 12, 6, 10, PAPER)
    elif kind == "down":
        rect(draw, x + 33, y + 6, 6, 10, PAPER)
        rect(draw, x + 30, y + 16, 12, 4, PAPER)
        rect(draw, x + 33, y + 19, 6, 4, PAPER)
        rect(draw, x + 35, y + 22, 2, 4, PAPER)
    elif kind == "ok":
        rect(draw, x + 24, y + 6, 24, 16, PAPER)
        rect(draw, x + 27, y + 9, 18, 10, INK)
        rect(draw, x + 30, y + 13, 4, 4, PAPER)
        rect(draw, x + 34, y + 15, 8, 4, PAPER)
        rect(draw, x + 40, y + 9, 4, 8, PAPER)
    elif kind == "pause":
        rect(draw, x + 28, y + 6, 6, 16, PAPER)
        rect(draw, x + 38, y + 6, 6, 16, PAPER)


def nav(draw: ImageDraw.ImageDraw, kinds: tuple[str, str, str]) -> None:
    for x, kind in zip((8, 84, 160), kinds):
        rect(draw, x, 286, 72, 28, INK)
        nav_icon(draw, x, kind)


def sprout(draw: ImageDraw.ImageDraw) -> None:
    rect(draw, 16, 18, 12, 5, GREEN)
    rect(draw, 20, 12, 5, 10, GREEN)
    rect(draw, 25, 20, 5, 5, GREEN)


def sun(draw: ImageDraw.ImageDraw) -> None:
    rect(draw, 207, 70, 8, 2, SUN)
    rect(draw, 203, 72, 16, 3, SUN)
    rect(draw, 201, 75, 20, 9, SUN)
    rect(draw, 203, 84, 16, 3, SUN)
    rect(draw, 207, 87, 8, 2, SUN)


def calendar_screen() -> Image.Image:
    image = Image.new("RGB", (240, 320), PAPER)
    draw = ImageDraw.Draw(image)
    status_icons(draw, connected=True)
    digits(draw, 13, 12, 4, "2026", fill=INK)
    digit(draw, 56, 51, 8, 9, INK)
    mountain = Image.open(ASSETS / "calendar-mountain.png").convert("RGB")
    cat = Image.open(ASSETS / "calendar-cat.png").convert("RGB")
    image.paste(mountain, (116, 76))
    sun(draw)
    center(draw, (94, 68, 122, 93), "月", INK, font(18))
    center(draw, (119, 47, 227, 71), "农历 七月廿九", INK, font(12))
    center(draw, (119, 73, 227, 95), "白露", GREEN, font(15))
    image.paste(cat, (159, 236))

    weekdays = ("日", "一", "二", "三", "四", "五", "六")
    for i, day in enumerate(weekdays):
        center(draw, (10 + i * 31, 112, 34 + i * 31, 132), day,
               RUST if i in (0, 6) else INK, font(13))

    year, month, selected = 2026, 9, 10
    first = (monthrange(year, month)[0] + 1) % 7  # Python Monday-first -> Sunday-first.
    total = monthrange(year, month)[1]
    selected_index = first + selected - 1
    marker_x = 10 + (selected_index % 7) * 31
    marker_y = 136 + (selected_index // 7) * 23
    draw.rounded_rectangle((marker_x, marker_y, marker_x + 24, marker_y + 24), radius=4,
                           fill=GREEN)
    for slot in range(first, first + total):
        day = slot - first + 1
        x = 10 + (slot % 7) * 31
        y = 137 + (slot // 7) * 23
        color = PAPER if day == selected else (RUST if slot % 7 in (0, 6) else INK)
        center(draw, (x, y, x + 25, y + 23), str(day), color, font(13, mono=True))
    nav(draw, ("up", "down", "ok"))
    return image


def pomodoro_screen() -> Image.Image:
    image = Image.new("RGB", (240, 320), PAPER)
    draw = ImageDraw.Draw(image)
    status_icons(draw, connected=True)
    sprout(draw)
    center(draw, (53, 12, 149, 38), "专注中", GREEN_DARK, font(16))
    center(draw, (66, 41, 176, 63), "第 1 / 4 轮", MUTED, font(13))
    digit(draw, 12, 89, 8, 2)
    digit(draw, 57, 89, 8, 4)
    colon(draw, 102, 89, 8)
    digit(draw, 117, 89, 8, 5)
    digit(draw, 162, 89, 8, 9)
    scene = Image.open(ASSETS / "pomodoro-scene-focus.png").convert("RGB")
    image.paste(scene, (5, 168))
    nav(draw, ("up", "down", "pause"))
    return image


def poster(title: str, subtitle: str, screen: Image.Image, footnote: str, destination: Path) -> None:
    canvas = Image.new("RGB", (1152, 1536), PAPER)
    draw = ImageDraw.Draw(canvas)
    draw.text((66, 82), title, fill=INK, font=font(50))
    draw.text((68, 169), subtitle, fill="#3F7A61", font=font(26))
    scaled = screen.resize((720, 960), Image.Resampling.NEAREST)
    rect(draw, 208, 270, 736, 976, "#FFFFFF")
    canvas.paste(scaled, (216, 278))
    draw.text((68, 1452), footnote, fill=RUST, font=font(20))
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, optimize=True)


def main() -> None:
    poster("月历", "Monthly Calendar", calendar_screen(),
           "月历 · 2026 年 9 月 · 已连接 Wi-Fi", OUT / "01-month-calendar.png")
    poster("专注进行时", "Focus Countdown", pomodoro_screen(),
           "24:59 · 专注中 · 站立猫陪伴", OUT / "03-focus-countdown.png")


if __name__ == "__main__":
    main()
