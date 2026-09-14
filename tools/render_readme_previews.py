#!/usr/bin/env python3
"""Render the three README posters from the current 240 x 320 UI contract."""
from __future__ import annotations

import re
from calendar import monthrange
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/assets/cat-focus-calendar/publish-3x4"
ART = ROOT / "assets/images/pixel-clock"

PAPER = "#F5F0E3"
POMO_CREAM = "#F7F3E7"
INK = "#17263A"
GREEN = "#506A4D"
GREEN_DARK = "#304B38"
RUST = "#B65B3F"
RED = "#A13127"
TOMATO = "#E23B32"
TOMATO_DARK = "#BE2D2A"
LEAF = "#7CB342"
STEM = "#6B4423"
MUTED = "#7C7A70"
SHADOW = "#D8CDB6"
SUN = "#EFD8A4"
CLOUD = "#C8C2B4"

MONTHS = (
    "", "正月", "二月", "三月", "四月", "五月", "六月",
    "七月", "八月", "九月", "十月", "冬月", "腊月",
)
DAYS = (
    "", "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十",
)
TERMS = (
    "冬至", "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明",
    "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋",
    "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪",
)
STEMS = ("甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸")
BRANCHES = ("子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥")

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

POSTER_FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
SCREEN_FONT = Path.home() / "Library/Fonts/LXGWWenKai-Medium.ttf"
MONO_FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf"


def load_font(size: int, *, poster: bool = False, mono: bool = False) -> ImageFont.FreeTypeFont:
    candidate = POSTER_FONT if poster else (MONO_FONT if mono else SCREEN_FONT)
    if not candidate.exists():
        candidate = POSTER_FONT
    return ImageFont.truetype(str(candidate), size=size)


def rect(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int, fill: str) -> None:
    draw.rectangle((x, y, x + w - 1, y + h - 1), fill=fill)


def label(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], value: str,
          fill: str, size: int, *, align: str = "center", mono: bool = False) -> None:
    x, y, w, h = box
    if align == "left":
        draw.text((x, y + h // 2), value, fill=fill, font=load_font(size, mono=mono), anchor="lm")
    elif align == "right":
        draw.text((x + w, y + h // 2), value, fill=fill, font=load_font(size, mono=mono), anchor="rm")
    else:
        draw.text((x + w // 2, y + h // 2), value, fill=fill,
                  font=load_font(size, mono=mono), anchor="mm")


def digit(draw: ImageDraw.ImageDraw, x: int, y: int, scale: int, value: int,
          fill: str = INK) -> None:
    for row, bits in enumerate(DIGITS[value]):
        for col in range(5):
            if bits & (1 << (4 - col)):
                rect(draw, x + col * scale, y + row * scale, scale, scale, fill)


def colon(draw: ImageDraw.ImageDraw, x: int, y: int, scale: int, fill: str = INK) -> None:
    rect(draw, x + 2 * scale, y + 2 * scale, scale, scale, fill)
    rect(draw, x + 2 * scale, y + 4 * scale, scale, scale, fill)


def status_icons(draw: ImageDraw.ImageDraw, background: str) -> None:
    for x, y, w, h in ((186, 7, 3, 3), (191, 5, 3, 3), (196, 5, 3, 3), (201, 7, 3, 3),
                       (189, 11, 12, 4), (191, 14, 8, 3)):
        rect(draw, x, y, w, h, GREEN)
    rect(draw, 207, 10, 28, 14, INK)
    rect(draw, 211, 13, 20, 8, background)
    rect(draw, 235, 14, 3, 6, INK)
    for x in (213, 219, 225):
        rect(draw, x, 15, 5, 4, GREEN)


def nav_icon(draw: ImageDraw.ImageDraw, x: int, kind: str, y: int = 286) -> None:
    if kind == "up":
        for i in range(8):
            yy = y + 9 + i
            spread = 1 + i * 2
            rect(draw, x + 36 - spread - 2, yy, 6, 3, PAPER)
            rect(draw, x + 36 + spread - 4, yy, 6, 3, PAPER)
    elif kind == "down":
        for i in range(8):
            yy = y + 11 + i
            spread = 1 + (7 - i) * 2
            rect(draw, x + 36 - spread - 2, yy, 6, 3, PAPER)
            rect(draw, x + 36 + spread - 4, yy, 6, 3, PAPER)
    elif kind == "ok":
        rect(draw, x + 24, y + 6, 24, 16, PAPER)
        rect(draw, x + 27, y + 9, 18, 10, INK)
        rect(draw, x + 30, y + 13, 4, 4, PAPER)
        rect(draw, x + 34, y + 15, 8, 4, PAPER)
        rect(draw, x + 40, y + 9, 4, 8, PAPER)
    elif kind == "pomo":
        rect(draw, x + 31, y + 8, 10, 2, TOMATO)
        rect(draw, x + 28, y + 10, 16, 3, TOMATO)
        rect(draw, x + 26, y + 13, 20, 3, TOMATO)
        rect(draw, x + 25, y + 16, 22, 6, TOMATO)
        rect(draw, x + 26, y + 22, 20, 2, TOMATO)
        rect(draw, x + 28, y + 24, 16, 2, TOMATO)
        rect(draw, x + 31, y + 26, 10, 1, TOMATO)
        rect(draw, x + 40, y + 13, 6, 10, TOMATO_DARK)
        rect(draw, x + 42, y + 15, 4, 7, TOMATO_DARK)
        rect(draw, x + 29, y + 13, 4, 4, PAPER)
        rect(draw, x + 30, y + 14, 2, 2, PAPER)
        rect(draw, x + 21, y + 7, 7, 3, LEAF)
        rect(draw, x + 19, y + 8, 4, 2, LEAF)
        rect(draw, x + 26, y + 3, 5, 5, LEAF)
        rect(draw, x + 27, y + 1, 3, 3, LEAF)
        rect(draw, x + 33, y + 2, 6, 6, LEAF)
        rect(draw, x + 34, y + 1, 4, 2, LEAF)
        rect(draw, x + 41, y + 3, 5, 5, LEAF)
        rect(draw, x + 42, y + 1, 3, 3, LEAF)
        rect(draw, x + 44, y + 7, 7, 3, LEAF)
        rect(draw, x + 49, y + 8, 4, 2, LEAF)
        rect(draw, x + 35, y + 0, 2, 5, STEM)
        rect(draw, x + 36, y + 1, 2, 3, STEM)
    elif kind == "pause":
        rect(draw, x + 28, y + 6, 6, 16, PAPER)
        rect(draw, x + 38, y + 6, 6, 16, PAPER)


def nav(draw: ImageDraw.ImageDraw, kinds: tuple[str, str, str]) -> None:
    rect(draw, 14, 278, 212, 2, SHADOW)
    for x, kind in zip((8, 84, 160), kinds):
        rect(draw, x, 286, 72, 28, INK)
        nav_icon(draw, x, kind)


def compact_nav_icon(draw: ImageDraw.ImageDraw, x: int, kind: str, ox: int) -> None:
    y = 286
    cx = x + ox + 8
    if kind == "up":
        for i in range(4):
            yy = y + 9 + i
            spread = i * 2
            rect(draw, cx - 2 - spread, yy, 4, 2, PAPER)
            rect(draw, cx - 2 + spread, yy, 4, 2, PAPER)
    elif kind == "down":
        for i in range(4):
            yy = y + 11 + i
            spread = (3 - i) * 2
            rect(draw, cx - 2 - spread, yy, 4, 2, PAPER)
            rect(draw, cx - 2 + spread, yy, 4, 2, PAPER)
    else:
        for i in range(6):
            xx = x + ox + 2 + i
            spread = 1 + i
            rect(draw, xx, y + 12 - spread, 2, 4, PAPER)
            rect(draw, xx, y + 12 + spread - 1, 2, 4, PAPER)


def nav_zh(draw: ImageDraw.ImageDraw) -> None:
    rect(draw, 14, 278, 212, 2, SHADOW)
    icon_w, gap = 16, 6
    for x, kind, caption in zip((8, 84, 160), ("up", "down", "back"), ("前一天", "后一天", "返回")):
        rect(draw, x, 286, 72, 28, INK)
        text_w = len(caption) * 13
        group = icon_w + gap + text_w
        start = max(2, (72 - group) // 2)
        compact_nav_icon(draw, x, kind, start)
        label(draw, (x + start + icon_w + gap, 293, text_w, 16), caption, PAPER, 13, align="left")


def calendar_sun(draw: ImageDraw.ImageDraw) -> None:
    rect(draw, 196, 30, 8, 2, SUN)
    rect(draw, 192, 32, 16, 3, SUN)
    rect(draw, 190, 35, 20, 12, SUN)
    rect(draw, 192, 47, 16, 3, SUN)
    rect(draw, 196, 50, 8, 2, SUN)


def solar_ordinal(year: int, month: int, day: int) -> int:
    y, m = year, month
    if m <= 2:
        y -= 1
        m += 12
    days = 365 * y + y // 4 - y // 100 + y // 400 + (153 * (m - 3) + 2) // 5 + day - 1
    base_y = 1899
    base = 365 * base_y + base_y // 4 - base_y // 100 + base_y // 400 + (153 * 10 + 2) // 5
    return days - base


def load_calendar_tables() -> tuple[list[tuple[int, int, int, int]], list[tuple[int, int]]]:
    source = (ROOT / "main/lunar_table.c").read_text()
    lunar = [tuple(map(int, match)) for match in re.findall(
        r"\{(\d+)u,\s*(-?\d+),\s*(-?\d+),\s*(\d+)u\}", source
    )]
    terms = [tuple(map(int, match)) for match in re.findall(r"\{(\d+)u,\s*(\d+)u\}", source)]
    return lunar, terms


LUNAR_TABLE, TERM_TABLE = load_calendar_tables()


def lunar_for(year: int, month: int, day: int) -> tuple[bool, int, int]:
    target = solar_ordinal(year, month, day)
    record = max(row for row in LUNAR_TABLE if row[0] <= target)
    lunar_day = target - record[0] + 1
    return record[2] < 0, abs(record[2]), lunar_day


def term_for(year: int, month: int, day: int) -> str:
    target = solar_ordinal(year, month, day)
    return next((TERMS[index] for ordinal, index in TERM_TABLE if ordinal == target), "")


def weekday_sunday_first(year: int, month: int, day: int) -> int:
    if month < 3:
        month += 12
        year -= 1
    k, j = year % 100, year // 100
    h = (day + (13 * (month + 1)) // 5 + k + k // 4 + j // 4 + 5 * j) % 7
    return (h + 6) % 7


def lunar_label(year: int, month: int, day: int) -> str:
    leap, lunar_month, lunar_day = lunar_for(year, month, day)
    return f"农历 {'闰' if leap else ''}{MONTHS[lunar_month]}{DAYS[lunar_day]}"


def lunar_cell(year: int, month: int, day: int) -> str:
    leap, lunar_month, lunar_day = lunar_for(year, month, day)
    if lunar_day == 1:
        return f"{'闰' if leap else ''}{MONTHS[lunar_month]}"
    return DAYS[lunar_day]


def parse_rgb565(path: Path, width: int, height: int) -> Image.Image:
    source = path.read_text()
    match = re.search(r"uint8_t\s+\w+_map\[\]\s*=\s*\{(.*?)\};", source, re.S)
    if not match:
        raise ValueError(f"RGB565 map missing in {path}")
    data = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", match.group(1))]
    if len(data) != width * height * 2:
        raise ValueError(f"unexpected RGB565 data length in {path}: {len(data)}")
    pixels = []
    for offset in range(0, len(data), 2):
        packed = data[offset] | (data[offset + 1] << 8)
        red = (packed >> 11) & 31
        green = (packed >> 5) & 63
        blue = packed & 31
        pixels.append(((red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2)))
    image = Image.new("RGB", (width, height))
    image.putdata(pixels)
    return image


def calendar_screen(year: int = 2026, month: int = 9, selected: int = 7) -> Image.Image:
    image = Image.new("RGB", (240, 320), PAPER)
    draw = ImageDraw.Draw(image)
    status_icons(draw, PAPER)
    label(draw, (8, 8, 160, 28), f"{year}年{month}月", INK, 22, align="left")
    summary = lunar_label(year, month, selected)
    term = term_for(year, month, selected)
    label(draw, (8, 38, 224, 22), f"{summary}{' ' + term if term else ''}", INK, 16, align="left")

    calendar_sun(draw)
    image.paste(Image.open(ART / "calendar-mountain.png").convert("RGB"), (118, 250))
    image.paste(Image.open(ART / "calendar-cat.png").convert("RGB"), (159, 236))

    for index, value in enumerate("日一二三四五六"):
        label(draw, (10 + index * 31, 60, 28, 20), value,
              RUST if index in (0, 6) else INK, 16)

    first = weekday_sunday_first(year, month, 1)
    total = monthrange(year, month)[1]
    for slot in range(42):
        day = slot - first + 1
        if not 1 <= day <= total:
            continue
        x = 10 + (slot % 7) * 31
        y = 82 + (slot // 7) * 28
        selected_now = day == selected
        if selected_now:
            draw.rounded_rectangle((x, y, x + 28, y + 26), radius=4, fill=GREEN)
        weekend = slot % 7 in (0, 6)
        day_color = PAPER if selected_now else (RUST if weekend else INK)
        lunar_color = PAPER if selected_now else MUTED
        label(draw, (x, y + 1, 29, 16), str(day), day_color, 14, mono=True)
        label(draw, (x, y + 15, 29, 14), lunar_cell(year, month, day), lunar_color, 11)
    nav(draw, ("up", "down", "pomo"))
    return image


def banner_frame(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    rect(draw, x, y, 28, 90, RED)
    rect(draw, x + 2, y + 2, 24, 86, PAPER)
    for cx, cy in ((x + 1, y + 1), (x + 24, y + 1), (x + 1, y + 86), (x + 24, y + 86)):
        rect(draw, cx, cy, 3, 3, RED)


def banner_text(draw: ImageDraw.ImageDraw, x: int, y: int, text_value: str) -> None:
    for index, character in enumerate(text_value):
        label(draw, (x + 2, y + 12 + index * 17, 24, 17), character, RED, 16)


def banner(draw: ImageDraw.ImageDraw, x: int, y: int, text_value: str) -> None:
    banner_frame(draw, x, y)
    banner_text(draw, x, y, text_value)


def draw_frame(draw: ImageDraw.ImageDraw, x: int, y: int, width: int, height: int,
               color: str) -> None:
    rect(draw, x, y, width, 2, color)
    rect(draw, x, y + height - 2, width, 2, color)
    rect(draw, x, y, 2, height, color)
    rect(draw, x + width - 2, y, 2, height, color)


def ganzhi_label(year: int, month: int, day: int) -> str:
    target = solar_ordinal(year, month, day)
    latest_jie = max(
        (ordinal, index) for ordinal, index in TERM_TABLE
        if ordinal <= target and index % 2 == 1
    )[1]
    month_number = 12 if latest_jie == 1 else (latest_jie - 3) // 2 + 1
    year_stem = (year - 4) % 10
    year_branch = (year - 4) % 12
    yin_stem = (2, 4, 6, 8, 0)[year_stem % 5]
    month_stem = (yin_stem + month_number - 1) % 10
    month_branch = (month_number + 1) % 12
    return f"{STEMS[year_stem]}{BRANCHES[year_branch]}年 {STEMS[month_stem]}{BRANCHES[month_branch]}月"


def mix_u32(value: int) -> int:
    value &= 0xFFFFFFFF
    value ^= (value << 13) & 0xFFFFFFFF
    value ^= value >> 17
    value ^= (value << 5) & 0xFFFFFFFF
    value &= 0xFFFFFFFF
    return value or 1


def almanac_items(year: int, month: int, day: int, bank: tuple[str, ...], salt: int,
                   *, avoid_alike: bool = False) -> tuple[str, str]:
    seed = mix_u32(year * 10000 + month * 100 + day + salt * 131)
    picked: list[str] = []
    used: set[int] = set()
    for attempt in range(len(bank) * 4):
        if len(picked) == 2:
            break
        seed = mix_u32(seed + attempt * 17)
        index = seed % len(bank)
        if index in used:
            continue
        candidate = bank[index]
        if avoid_alike and picked:
            sleep_pair = any(value in candidate for value in ("熬夜", "晚睡", "赖床")) and any(
                value in picked[0] for value in ("熬夜", "晚睡", "赖床")
            )
            video_pair = any(value in candidate for value in ("刷视频", "刷手机", "刷剧")) and any(
                value in picked[0] for value in ("刷视频", "刷手机", "刷剧")
            )
            work_pair = any(value in candidate for value in ("加班", "赶工")) and any(
                value in picked[0] for value in ("加班", "赶工")
            )
            if sleep_pair or video_pair or work_pair:
                continue
        used.add(index)
        picked.append(candidate)
    for index, candidate in enumerate(bank):
        if len(picked) == 2:
            break
        if index not in used:
            picked.append(candidate)
    return picked[0], picked[1]


def chinese_screen(year: int = 2026, month: int = 9, day: int = 7) -> Image.Image:
    image = Image.new("RGB", (240, 320), PAPER)
    draw = ImageDraw.Draw(image)

    status_icons(draw, PAPER)
    banner(draw, 10, 75, "万事顺遂")
    banner_frame(draw, 202, 75)

    mountain = Image.open(ART / "calendar-mountain.png").convert("RGB")
    cat = Image.open(ART / "calendar-cat.png").convert("RGB")
    image.paste(mountain, (145, 148))
    rect(draw, 210, 140, 6, 2, SUN)
    rect(draw, 207, 142, 12, 2, SUN)
    rect(draw, 205, 144, 16, 8, SUN)
    rect(draw, 207, 152, 12, 2, SUN)
    rect(draw, 210, 154, 6, 2, SUN)
    image.paste(cat, (158, 175))
    banner_text(draw, 202, 75, "专注当下")

    well_x, well_w, scale, gap, digit_y = 38, 164, 12, 8, 78
    dw = 5 * scale
    if day < 10:
        digit(draw, well_x + (well_w - dw) // 2, digit_y, scale, day, RED)
    else:
        pair = dw + gap + dw
        x0 = well_x + (well_w - pair) // 2
        digit(draw, x0, digit_y, scale, day // 10, RED)
        digit(draw, x0 + dw + gap, digit_y, scale, day % 10, RED)

    label(draw, (0, 4, 240, 24), f"{year}年{month}月", INK, 22)
    weekdays = ("星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六")
    leap, lm, ld = lunar_for(year, month, day)
    lunar = f"农历{'闰' if leap else ''}{MONTHS[lm]}{DAYS[ld]}"
    label(draw, (0, 30, 240, 18), f"{weekdays[weekday_sunday_first(year, month, day)]}  {lunar}", GREEN, 13)
    term = term_for(year, month, day)
    if term:
        label(draw, (0, 48, 240, 16), f"今日{term}", GREEN, 13)

    draw_frame(draw, 20, 178, 132, 26, RED)
    rect(draw, 16, 186, 6, 10, RED)
    rect(draw, 146, 186, 6, 10, RED)
    label(draw, (24, 178, 124, 26), ganzhi_label(year, month, day), INK, 16)

    draw_frame(draw, 12, 212, 216, 52, RED)
    rect(draw, 118, 216, 2, 44, RED)
    rect(draw, 20, 227, 22, 22, RED)
    label(draw, (20, 227, 22, 22), "宜", PAPER, 16)
    rect(draw, 128, 227, 22, 22, GREEN)
    label(draw, (128, 227, 22, 22), "忌", PAPER, 16)

    yi_bank = ("早起", "写信", "整理", "运动", "浇花", "记账", "练字", "听歌", "复盘", "散步", "做饭", "深呼吸", "收纳", "喝水", "拉伸", "专注")
    ji_bank = ("拖延", "熬夜", "赖床", "比较", "硬撑", "空腹", "赶工", "起哄", "刷剧", "内耗", "透支", "冷饭", "加塞", "买闲")
    yi = almanac_items(year, month, day, yi_bank, 1)
    ji = almanac_items(year, month, day, ji_bank, 2, avoid_alike=True)
    yi_font = load_font(13)
    for col_x, lines in ((46, yi), (152, ji)):
        yy = 224
        for line in lines:
            bbox = yi_font.getbbox(line)
            lw = bbox[2] - bbox[0]
            draw.text((col_x + (70 - lw) / 2, yy), line, fill=INK, font=yi_font)
            yy += 14
    nav_zh(draw)
    return image


def pomodoro_screen() -> Image.Image:
    image = Image.new("RGB", (240, 320), POMO_CREAM)
    draw = ImageDraw.Draw(image)
    status_icons(draw, POMO_CREAM)
    label(draw, (0, 12, 240, 26), "专注中", GREEN_DARK, 16)
    label(draw, (0, 41, 240, 22), "第 1 / 4 轮", MUTED, 16)
    digit(draw, 12, 89, 8, 2)
    digit(draw, 57, 89, 8, 4)
    colon(draw, 102, 89, 8)
    digit(draw, 140, 89, 8, 5)
    digit(draw, 185, 89, 8, 9)
    scene = parse_rgb565(ROOT / "main/assets/folotoy_pomodoro_scene_focus.c", 230, 107)
    image.paste(scene, (5, 168))
    nav(draw, ("up", "down", "pause"))
    return image


def poster(title: str, subtitle: str, screen: Image.Image, footnote: str,
           destination: Path) -> None:
    canvas = Image.new("RGB", (1152, 1536), PAPER)
    draw = ImageDraw.Draw(canvas)
    draw.text((66, 82), title, fill=INK, font=load_font(50, poster=True))
    draw.text((68, 169), subtitle, fill="#3F7A61", font=load_font(26, poster=True))
    scaled = screen.resize((720, 960), Image.Resampling.NEAREST)
    rect(draw, 208, 270, 736, 976, "#FFFFFF")
    canvas.paste(scaled, (216, 278))
    draw.text((68, 1452), footnote, fill=RUST, font=load_font(20, poster=True))
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, optimize=True)


def main() -> None:
    poster("月历", "Monthly Calendar", calendar_screen(),
           "公历 / 农历双行 · 节气仅当天 · 已连接 Wi-Fi", OUT / "01-month-calendar.png")
    poster("中国日历", "Chinese Day Page", chinese_screen(),
           "撕页日历 · 节气仅当天 · 宜忌各两项", OUT / "04-chinese-calendar.png")
    poster("专注进行时", "Focus Countdown", pomodoro_screen(),
           "24:59 · 专注中 · 奶油猫咪主题", OUT / "03-focus-countdown.png")


if __name__ == "__main__":
    main()
