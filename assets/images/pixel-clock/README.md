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
| `pomodoro-scene-forest-rest.png` / `pomodoro-scene-forest-focus.png` | 230 × 107 RGB | `folotoy_pomodoro_scene_forest.c` / `_forest_focus.c` | Ragdoll Pomodoro theme. Same warm lamp-lit room; rest sleeping / focus sitting. |
| `pomodoro-scene-night-rest.png` / `pomodoro-scene-night-focus.png` | 230 × 107 RGB | `folotoy_pomodoro_scene_night.c` / `_night_focus.c` | Exotic-shorthair (Garfield-type) Pomodoro theme. Same warm lamp-lit room; rest sleeping / focus sitting. |
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

## Focus-art repair (2026-09-12)

The ragdoll and Exotic-shorthair focus PNGs were edited with imagegen from their resting scenes, then resized to 230x107. These are generated illustrations, not device captures. Preserve the current PNGs as inputs; do not rerun the experimental `scratch/build_perfect_assets.py` on them.

Use `python3 tools/export_pomodoro_assets.py` to export all six opaque scenes and `python3 tools/export_pomodoro_assets.py --check` to verify their dimensions and exact RGB565 bytes (requires Pillow). This avoids alpha multiplication darkening opaque colors. `python3 tools/render_ui_previews.py` renders all three skins in focus and rest from the C arrays. Generated focus rooms can have small detail/lighting differences; device appearance is not yet accepted.
