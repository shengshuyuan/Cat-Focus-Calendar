#!/usr/bin/env python3
from __future__ import annotations
import re
from calendar import monthrange
from collections import Counter
from datetime import date
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/previews"
FONT = Path("/Users/shengshuyuan/Library/Fonts/LXGWWenKai-Medium.ttf")
W, H = 240, 320
SCENE_W, SCENE_H = 230, 107
PAPER = (245, 240, 227)
INK = (23, 38, 58)
GREEN = (80, 106, 77)
GREEN_DARK = (48, 75, 56)
RUST = (182, 91, 63)
MUTED = (124, 122, 112)
NAV = (23, 38, 58)

MONTHS = ["", "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "冬月", "腊月"]
DAYS = ["", "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
        "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
        "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"]
TERMS = ["冬至", "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明",
         "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋",
         "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪"]
THEMES = {
    "cream":  dict(rest="folotoy_pomodoro_scene.c", focus="folotoy_pomodoro_scene_focus.c", bg=0xF7F3E7),
    "forest": dict(rest="folotoy_pomodoro_scene_forest.c", focus="folotoy_pomodoro_scene_forest_focus.c", bg=0xD6CBA5),
    "night":  dict(rest="folotoy_pomodoro_scene_night.c", focus="folotoy_pomodoro_scene_night_focus.c", bg=0xDEE3CE),
}

def font(size):
    return ImageFont.truetype(str(FONT), size=size)

def hex_rgb(h):
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255)

def solar_ordinal(year, month, day):
    y, m = year, month
    if m <= 2:
        y -= 1
        m += 12
    days = 365 * y + y // 4 - y // 100 + y // 400 + (153 * (m - 3) + 2) // 5 + day - 1
    base_y = 1899
    base = 365 * base_y + base_y // 4 - base_y // 100 + base_y // 400 + (153 * 10 + 2) // 5
    return days - base

def load_tables():
    text = (ROOT / "main/lunar_table.c").read_text()
    months = [(int(a), int(b), int(c), int(d)) for a, b, c, d in
              re.findall(r"\{(\d+)u,\s*(-?\d+),\s*(-?\d+),\s*(\d+)u\}", text)]
    # term table: {solar_day, index}
    terms = [(int(a), int(b)) for a, b in re.findall(r"\{(\d+)u,\s*(\d+)u\}", text)]
    return months, terms

MONTH_TAB, TERM_TAB = load_tables()

def lunar_lookup(y, m, d):
    target = solar_ordinal(y, m, d)
    lo, hi = 0, len(MONTH_TAB)
    while lo < hi:
        mid = (lo + hi) // 2
        if MONTH_TAB[mid][0] <= target:
            lo = mid + 1
        else:
            hi = mid
    rec = MONTH_TAB[lo - 1]
    lunar_day = target - rec[0] + 1
    leap = rec[2] < 0
    lm = -rec[2] if leap else rec[2]
    return leap, lm, lunar_day

def term_on_day(y, m, d):
    target = solar_ordinal(y, m, d)
    lo, hi = 0, len(TERM_TAB)
    while lo < hi:
        mid = (lo + hi) // 2
        if TERM_TAB[mid][0] < target:
            lo = mid + 1
        else:
            hi = mid
    if lo >= len(TERM_TAB) or TERM_TAB[lo][0] != target:
        return ""
    return TERMS[TERM_TAB[lo][1]]

def weekday_sun0(y, m, d):
    if m < 3:
        m += 12
        y -= 1
    k, j = y % 100, y // 100
    h = (d + (13 * (m + 1)) // 5 + k + k // 4 + j // 4 + 5 * j) % 7
    return (h + 6) % 7

def parse_rgb565(path):
    text = path.read_text()
    m = re.search(r"uint8_t\s+\w+_map\[\]\s*=\s*\{(.*?)\};", text, re.S)
    nums = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(1))]
    pixels = []
    for i in range(0, len(nums), 2):
        p = nums[i] | (nums[i + 1] << 8)
        r = (p >> 11) & 31
        g = (p >> 5) & 63
        b = p & 31
        pixels.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    img = Image.new("RGB", (SCENE_W, SCENE_H))
    img.putdata(pixels)
    return img

def status_icons(draw, bg):
    # battery/wifi as simple ink on whatever bg
    draw.rectangle((174, 12, 177, 15), fill=GREEN)
    draw.rectangle((180, 9, 183, 12), fill=GREEN)
    draw.rectangle((186, 12, 189, 15), fill=GREEN)
    draw.rectangle((180, 16, 183, 19), fill=GREEN)
    draw.rectangle((207, 10, 234, 23), fill=INK)
    draw.rectangle((211, 13, 230, 20), fill=bg)
    draw.rectangle((235, 14, 237, 19), fill=INK)
    draw.rectangle((213, 15, 217, 18), fill=GREEN)
    draw.rectangle((219, 15, 223, 18), fill=GREEN)
    draw.rectangle((225, 15, 229, 18), fill=GREEN)

def nav(draw):
    draw.rectangle((0, 292, 239, 319), fill=NAV)

def render_calendar(y, m, d, name):
    img = Image.new("RGB", (W, H), PAPER)
    draw = ImageDraw.Draw(img)
    status_icons(draw, PAPER)
    nav(draw)
    title = f"{y}年{m}月"
    leap, lm, ld = lunar_lookup(y, m, d)
    lunar = f"农历 {'闰' if leap else ''}{MONTHS[lm]}{DAYS[ld]}"
    term = term_on_day(y, m, d)
    if term:
        lunar = f"{lunar} {term}"
    draw.text((8, 8), title, font=font(22), fill=INK)
    draw.text((8, 38), lunar, font=font(16), fill=INK)
    wds = "日一二三四五六"
    for i, ch in enumerate(wds):
        fill = RUST if i in (0, 6) else INK
        draw.text((10 + i * 31 + 6, 60), ch, font=font(16), fill=fill)
    first = weekday_sun0(y, m, 1)
    total = monthrange(y, m)[1]
    for i in range(42):
        day = i - first + 1
        if day < 1 or day > total:
            continue
        x = 10 + (i % 7) * 31
        cy = 82 + (i // 7) * 28
        selected = day == d
        weekend = i % 7 in (0, 6)
        if selected:
            draw.rounded_rectangle((x, cy, x + 28, cy + 26), radius=4, fill=GREEN)
        leap2, lm2, ld2 = lunar_lookup(y, m, day)
        cap = MONTHS[lm2] if ld2 == 1 else DAYS[ld2]
        dc = PAPER if selected else (RUST if weekend else INK)
        lc = PAPER if selected else MUTED
        draw.text((x + 4, cy + 1), str(day), font=font(14), fill=dc)
        draw.text((x + 1, cy + 14), cap, font=font(11), fill=lc)
    img.save(OUT / name)
    print("wrote", name, lunar)

def render_pomo(theme, pose, name):
    bg = hex_rgb(THEMES[theme]["bg"])
    img = Image.new("RGB", (W, H), bg)
    draw = ImageDraw.Draw(img)
    status_icons(draw, bg)
    nav(draw)
    label = "专注中" if pose == "focus" else "休息中"
    draw.text((120, 24), label, font=font(16), fill=GREEN_DARK, anchor="mm")
    draw.text((120, 52), "第 1 / 4 轮", font=font(16), fill=MUTED, anchor="mm")
    draw.text((120, 120), "25 : 00", font=font(28), fill=INK, anchor="mm")
    scene = parse_rgb565(ROOT / "main/assets" / THEMES[theme][pose])
    img.paste(scene, (5, 168))
    img.save(OUT / name)
    print("wrote", name)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    render_calendar(2026, 9, 11, "calendar_2026-09-11.png")
    render_calendar(2026, 9, 7, "calendar_2026-09-07_bailu.png")
    render_calendar(2026, 10, 1, "calendar_2026-10-01.png")
    render_calendar(2026, 10, 8, "calendar_2026-10-08_hanlu.png")
    render_calendar(2026, 10, 15, "calendar_2026-10-15.png")
    for th in THEMES:
        render_pomo(th, "rest", f"pomo_{th}_rest.png")
        render_pomo(th, "focus", f"pomo_{th}_focus.png")

if __name__ == "__main__":
    main()
