<p align="right">
  <a href="pixel-clock-v1.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pixel Clock v1

Locked Cat Focus Calendar page, layer, month-browse, and SoftAP rules live in [cat-focus-calendar.md](./cat-focus-calendar.md).

This branch implements the first visual and interaction slice for the FoloToy AI Passport: a paper-toned pixel calendar, a Chinese tear-off-style daily page, and a cat-themed Pomodoro screen at the panel's native 240 × 320 portrait size.

## Included

- Calendar is the boot screen. It shows the selected Gregorian date, lunar date, and solar term together; `UP` and `DOWN` move between months, and `OK` opens Pomodoro directly.
- Long-press `UP` from the calendar to open the Chinese tear-off-style daily page. It shares the calendar's selected date, shows weekday, lunar date, solar term, ganzhi, and personal focus suggestions; `UP` and `DOWN` move one day backward and forward, while `OK` returns to the calendar. Its suggestions are not an authoritative traditional almanac.
- The calendar has an offline lunar-month and 24-solar-term table covering 1900–2100, so its lunar information does not depend on Wi-Fi.
- Pomodoro starts at 25 minutes. In the idle state, `UP` cycles 15, 25, and 45 minutes; `DOWN` returns to the calendar without pausing an active timer. `OK` starts, pauses, resumes, or starts the prompted break.
- A long `OK` returns from Pomodoro to the calendar.
- A long `DOWN` from the calendar opens the Wi-Fi provisioning page. The device starts a SoftAP portal at `http://192.168.4.1`; the phone joins that hotspot, submits 2.4 GHz credentials, and NTP corrects the device clock.
- Pomodoro state and completed-session counters use the reference NVS store and recover running work as paused after restart.
- A small generated LVGL font contains the Chinese glyphs used by these screens. The font source is LXGW WenKai Medium installed locally during development; only the generated C glyph table is tracked.
- When regenerating, always pass `--no-compress` (this repo leaves `CONFIG_LV_USE_FONT_COMPRESSED` off) and keep the export symbol `folotoy_font`. Missing glyphs or compressed bitmaps without decompress support render Chinese as blank. Extend `--symbols` whenever copy changes, then verify the Wi-Fi page, solar terms, and pomodoro strings on device.
- The static cat, plant, books, mountain, and branch art uses RGB565 assets in Flash. This keeps detailed pixel scenes faithful to the reference without building hundreds of LVGL objects; see [pixel-clock art assets](../assets/images/pixel-clock/README.md).
- During a running focus interval, the lower Pomodoro scene changes from the sleeping calico cat to the matching awake standing-calico asset. Idle, paused, and break states keep the resting scene.
- Bottom navigation uses three dark capsule buttons with state-appropriate pixel icons; the four off-design progress squares are removed.
- After ten minutes without a `CLICK` or `LONG` event, the firmware sets only the display backlight to zero. It preserves the current page and Pomodoro state; the first eligible button event wakes the backlight without navigating, and the next event operates the UI. This is not display-controller sleep or deep sleep.

## Lunar and solar-term data

The development tool uses `lunar-python` v1.4.8 (MIT) to generate a static C table. The device only performs a binary search and has no Python runtime or third-party data request. As a correctness check, 2026-09-09 is lunar month seven, day twenty-eight, and White Dew in 2026 is September 7; the lunar label in the reference image is a visual mock value.

## Wi-Fi setup

The firmware does not contain the user's home SSID or password. From the calendar, long-press `DOWN` to open the Wi-Fi provisioning page. The screen shows the SoftAP name, such as `FoloToy-12AB34`. On a phone, join that 2.4 GHz hotspot and open `http://192.168.4.1`, then choose the home network and enter its password. ESP-IDF stores the received credentials in its Wi-Fi NVS. After the screen shows a connected state, NTP is requested from `ntp.aliyun.com` and `pool.ntp.org`; long-press `OK` returns to the calendar.

## Device test snapshot

On 2026-09-10 the earlier firmware snapshot was built with ESP-IDF v5.5.3 and flashed to an ESP32-C3 over `/dev/cu.usbmodem1101` with `idf.py flash`. The incremental flash erased only the bootloader, partition table, and factory-app ranges; the protected `cardid` partition at `0x356000` was not touched. Later SoftAP, calendar-grid, and Chinese-page layer changes have not been rebuilt in this environment because the required ESP-IDF Python virtual environment is absent. Physical visual inspection and pressing each key remain the final manual acceptance step for calendar month switching, Chinese daily date navigation, the Pomodoro start/pause flow, SoftAP provisioning, idle backlight wake, and protected flash layout.
