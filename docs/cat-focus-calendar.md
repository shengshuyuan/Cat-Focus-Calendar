<p align="right">
  <a href="cat-focus-calendar.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Cat Focus Calendar — product constraints

> Updated: 2026-09-11  
> Local repository: `ai-passport` (upstream `FoloToy/ai-passport`)  
> Main implementation: `main/clock_app.c`, `main/wifi_provision.c`, `main/clock_time.c`, `main/calendar_model.c`

This document records the locked page map, interactions, Chinese daily-page layer order, and flash-safety rules so later UI edits do not undo them.

The earlier pixel-clock slice is documented in [pixel-clock-v1.md](./pixel-clock-v1.md).

---

## 1. Product and hardware

| Item | Locked value |
|---|---|
| Product name | Cat Focus Calendar |
| Chip | ESP32-C3, 8 MB Flash |
| Display | 240 × 320 color panel |
| Buttons | UP / DOWN / OK; the left power control is a hard cut-off (firmware cannot see a tap) |
| Wi-Fi | 2.4 GHz only |
| NFC | On-board passive NTAG213, not wired to the MCU; firmware has no NFC driver |
| Audio | ES8311, usable for chimes |
| Battery | ~500 mAh; SOC from CW2017; UI shows a coarse 3-segment bar |

---

## 2. Pages

Four pages, all entered from the calendar:

| Page | Code | Enter | Leave |
|---|---|---|---|
| Calendar | `PAGE_CALENDAR` / `build_calendar` | Boot default | — |
| Chinese daily (tear-off) | `PAGE_CHINESE` / `build_chinese` | Calendar **long-press UP** | Short OK / long OK |
| Pomodoro | `PAGE_POMODORO` / `build_pomodoro` | Calendar **short OK** | Short DOWN back to calendar; long OK also returns |
| Wi-Fi provisioning | `PAGE_WIFI` / `build_wifi` | Calendar **long-press DOWN** | Long OK |

### 2.1 Calendar

- Shows year/month, the month grid, a lunar summary, today's solar-term name when it applies, and the selected-day marker.
- **Short UP/DOWN**: change month (see section 3).
- When the clock is unsynced, the bottom hint tells the user to long-press DOWN to provision.
- After about ten minutes without a key, only the backlight turns off; the first function-key event wakes the display and does not change page.

### 2.2 Chinese daily page — locked UI

- Large day digits, left/right couplets, mountain/cat art, ganzhi, and focus suggestions (yi/ji).
- **Short UP/DOWN**: previous / next day.
- Lunar date and solar term are **top-centered**; the solar term shows the "today" prefix **only on that term's date**.
- Yi and ji each show **2** items on separate lines. Do not add a third item.

**Layer order (back to front, locked)** — maintain only through `stack_chinese_layers()`:

1. Right couplet (focus-on-the-present copy)
2. Mountain `(145,148)` + cat `(158,175)` (**do not move these coordinates down**; the mountain may cover the lower edge of the right couplet)
3. Large day digits (about 6 px between the two digits: tens `x=50`, ones `x=126`, `y≈74`)
4. Top: year / weekday / lunar / solar term
5. Ganzhi frame
6. Yi/ji frame and copy

**Do not**

- Send the right couplet to the back of the whole screen with `lv_obj_move_background()` (the couplet disappears).
- Move the mountain/cat coordinates to dodge overlap; overlap is resolved only by z-order.

The left couplet is peer decoration; the right couplet must stay in the raise chain.

### 2.3 Pomodoro

- Three themes stored in NVS: cream cat, forest cabin, night desk.
- **Long-press UP** cycles the theme and shows a toast for about 1.5 s.
- Scene art is about `230×107` RGB565. Paper and digit colors follow the theme. **Focus and rest of the same skin share one background color** and only swap the cat scene (`scene_focus` / `scene_rest`).
- While idle, UP cycles duration; OK starts, pauses, or resumes; completion may play an ES8311 chime (mute is respected).
- Focus and rest may use different scene images.

### 2.4 Wi-Fi provisioning (SoftAP)

- BLE provisioning is replaced by a **SoftAP web page**: join the device hotspot and open `http://192.168.4.1`.
- 2.4 GHz only; credentials are written to Flash and reused for reconnect.
- The portal reports connect results through `/status`. After success the SoftAP stays up briefly, then stops, then NTP runs.
- Some networks (captive portals) can yield GOT_IP while NTP/WAN still fails, so the screen may remain unsynced. Retry with a home LAN or phone hotspot.

---

## 3. Date and month-browse logic

- The selected day `s_year/s_month/s_day` is shared by the calendar and the Chinese daily page.
- **Today anchor** `s_today_*`: captured at boot from the displayed day; refreshed after a successful time sync.
- **Month browse**
  - Leaving today's month selects **day 1** (do not carry the previous day number into the next month).
  - Returning to today's month restores **today** from the anchor (works even when RTC sync failed, using the boot-remembered date).

Implementation: `change_month()` plus `remember_today()` / `refresh_today_anchor()`.

---

## 4. Code map

| Module | Path |
|---|---|
| Pages / keys / layers | `main/clock_app.c` (`stack_chinese_layers`) |
| SoftAP provisioning | `main/wifi_provision.c` |
| Time / NVS / unsynced hint | `main/clock_time.c` |
| Lunar / solar terms / ganzhi | `main/calendar_model.c` + `main/lunar_table.c` |
| Chinese fonts | `main/fonts/folotoy_font.c` and `folotoy_font_{11,13,22}.c` (`--no-compress`; missing glyphs render blank) |
| RGB565 art | `main/assets/` |

---

## 5. Build and flash rules

- Toolchain: ESP-IDF **v5.5.3**, target esp32c3.
- **Default write is factory only**: `0x10000` ← `build/FoloToy-AI-Passport.bin`.
- **Do not** erase or write `cardid@0x356000`.
- Common serial port: `/dev/cu.usbmodem1101`.
- Confirm with the user before flashing; validate the software build first.
- The desktop merged image has been named `Cat-Focus-Calendar-full.bin` (refresh the package notes when publishing).

Factory-only example:

```bash
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem1101 -b 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x10000 build/FoloToy-AI-Passport.bin
```

---

## 6. Fonts and copy

- Adding or removing Chinese copy must extend `lv_font_conv --symbols` and keep `--no-compress`.
- The solar-term "today" prefix and SoftAP page copy need their glyphs in the font.
- After a copy change, check the Chinese daily page, the provisioning page, and Pomodoro theme names on device.

---

## 7. UI-edit checklist

1. Did `stack_chinese_layers` order change? If it must change, render a preview before flashing.
2. Are mountain/cat still at `(145,148)` / `(158,175)`?
3. Is the right couplet still visible (not sent to the back of the screen)?
4. Is ganzhi above the cat, and are the large digits above the mountain?
5. Is the solar term only on its date? Are yi/ji still 2+2?
6. Month browse: other months on day 1; returning to this month still selects today?
7. Does the flash write touch factory only?

---

## 8. 2026-09-11 change summary

- SoftAP provisioning replaced BLE; Wi-Fi credentials persist in Flash.
- Chinese daily page: solar term only on that date; lunar/term top-centered; yi/ji 2+2; layer order locked.
- Month selection uses the `s_today_*` anchor so returning to this month no longer jumps to day 1.
- Three Pomodoro themes, idle backlight, and the unsynced-time hint were already on the branch.
