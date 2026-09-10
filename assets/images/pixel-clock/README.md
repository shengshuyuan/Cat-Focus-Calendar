<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pixel Clock Art Assets

These source PNGs hold the static pixel-art scenes used by the 240 × 320
Pixel Clock pages. The dynamic time, Pomodoro state, lunar date, calendar
selection, battery level, and Wi-Fi state remain LVGL code.

| Source image | Size | Firmware output | Placement |
| --- | --- | --- | --- |
| `pomodoro-scene.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene.c` | Pomodoro lower scene for IDLE / BREAK_* / BREAK_PROMPT / REWARD: plant, lying cat, mug, and books. |
| `pomodoro-scene-focus.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene_focus.c` | Pomodoro lower scene for FOCUS_RUNNING / FOCUS_PAUSED / ABANDON_CONFIRM: plant, standing calico, FOCUS books, and mug. |
| `calendar-mountain.png` | 121 × 28 RGB | `main/assets/folotoy_calendar_mountain.c` | Calendar upper-right mountain and branch. |
| `calendar-cat.png` | 76 × 32 RGB | `main/assets/folotoy_calendar_cat.c` | Calendar lower-right cat and leaves. |

The original resting PNGs are reduced, nearest-neighbor crops derived from the
user-provided Pixel Clock visual reference on 2026-09-10. The focus PNG is the
user-provided standing-cat full scene from the same day, nearest-neighbor
scaled to 230×107 on paper background #F5F0E3. The C
assets use LVGL RGB565 descriptors generated with the repository's
`managed_components/lvgl__lvgl/scripts/LVGLImage.py`; they occupy Flash and
do not allocate an equivalent scene graph in the LVGL memory pool.

When replacing art, preserve the listed dimensions and regenerate the paired
C descriptor before building. Do not bake dates, personal Wi-Fi data, or other
dynamic information into the source images.
