<p align="right">
  <a href="coding-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Coding Conventions

- Write C with four-space indentation and K&R braces, following neighboring files. Use `snake_case`, `BSP_*` public constants, `s_` file-local state, `bsp_` public BSP APIs, and `demo_<feature>_<action>` demo entry points. Prefer `static` for internal symbols.
- Keep UI text and default documentation in English. Explanatory source comments may use Chinese while retaining established English technical terms.
- **CJK missing / blank text trap:** the baseline enables only LVGL Montserrat 14 and 20, which have no CJK glyphs. Chinese UTF-8 can render as tofu boxes *or entirely blank*; changing source encoding does not help. Before Chinese UI text:
  1. Build a glyph subset with `lv_font_conv` that covers every on-screen character (status copy, solar terms, punctuation), prefer subset over a full CJK face;
  2. Pass **`--no-compress` by default** — this repo leaves `CONFIG_LV_USE_FONT_COMPRESSED` off. A font with `bitmap_format = 1` (compressed) will sit in the binary but draw blank on device if decompression is disabled;
  3. Keep the exported symbol name aligned with code (`folotoy_font` for the pixel-clock path in `main/fonts/folotoy_font.c`) and list the file in `main/CMakeLists.txt`;
  4. After build, assert every UI CJK code point is in the font cmap, then verify page-by-page on hardware.
  Example: `npx lv_font_conv --font <LXGWWenKai-Medium.ttf> --size 16 --bpp 4 --no-compress --no-kerning --format lvgl --lv-include lvgl.h --range 0x20-0x7e --symbols '<needed CJK>' -o main/fonts/folotoy_font.c`
- Put reusable hardware behavior in `components/bsp`; keep menus, animations, product interaction, and validation pages in `main`.
- The `ui_pixel` theme (sky background, grass, title plate, mascot, ink-outlined panels) is part of the user interface, not a removable component. When trimming components or routing straight to a feature screen, keep the theme and build the screen through `ui_pixel_screen_create()` / `ui_pixel_panel_create()`.
- Show the battery level in the top-right corner of a user interface by default, unless the developer specifies a different placement or explicitly does not want it. Read it from `bsp_battery_soc()` (and `bsp_battery_mv()` where useful); render it as a small battery indicator or percentage in the top-right area of the screen, and degrade gracefully when it reads `-1` (unavailable). Place it where it does not overlap the existing cloud decoration (`add_cloud`, around `x≈188, y≈8`): use the clear sky space beside or below the cloud, or the very top-right edge, rather than covering the cloud.
- Document non-trivial functions, state, ownership, blocking behavior, task context, initialization order, failure values, register choices, timing, synchronization, and hardware-specific constants. Explain why, not merely what.
- Add or update tests with code changes. If automation is not practical, record the test gap and exact manual validation path.
- If adding a cache, define expiration and cleanup unless durable retention is explicitly justified.
- The ESP32-C3 has no PSRAM. Review internal RAM and largest-contiguous-block impact before increasing LVGL buffers, audio allocations, network state, or task stacks.
- **Watch power consumption.** This is a wearable powered by a small battery; keep it efficient. Avoid keeping the screen lit for long periods: dim or turn off the backlight, and return to a low-power state (light/deep sleep) whenever the screen is idle, so the device is not left displaying a bright screen while doing nothing. See the guidance on sleep in [`../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](../../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
