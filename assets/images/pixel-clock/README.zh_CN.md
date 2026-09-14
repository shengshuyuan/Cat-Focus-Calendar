<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 像素时钟插画资源

这些 PNG 保存 240 × 320 像素时钟页面中的固定像素插画。倒计时、番茄钟状态、农历、选中日期、电量和 Wi-Fi 状态仍由 LVGL 代码实时绘制。

| 源图 | 尺寸 | 固件输出 | 放置位置 |
| --- | --- | --- | --- |
| `pomodoro-scene.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene.c` | IDLE / BREAK_* / BREAK_PROMPT / REWARD 等休息或结束后的番茄钟下半部：绿植、趴姿猫、杯子和书。 |
| `pomodoro-scene-focus.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene_focus.c` | FOCUS_RUNNING / FOCUS_PAUSED / ABANDON_CONFIRM 专注相关状态的番茄钟下半部：绿植、站立三花猫、FOCUS 书与杯子。 |
| `pomodoro-scene-forest-rest.png` / `pomodoro-scene-forest-focus.png` | 230 × 107 RGB | `folotoy_pomodoro_scene_forest.c` / `_forest_focus.c` | 布偶主题。同一套暖光房间；休息睡姿 / 专注坐姿。 |
| `pomodoro-scene-night-rest.png` / `pomodoro-scene-night-focus.png` | 230 × 107 RGB | `folotoy_pomodoro_scene_night.c` / `_night_focus.c` | 加菲（异短橘猫）主题。同一套暖光房间；休息睡姿 / 专注坐姿。 |
| `calendar-mountain.png` | 121 × 28 RGB | `main/assets/folotoy_calendar_mountain.c` | 万年历右上角的山景与枝叶。 |
| `calendar-cat.png` | 76 × 32 RGB | `main/assets/folotoy_calendar_cat.c` | 万年历右下角的猫和枝叶。 |

原始睡猫 PNG 从用户在 2026-09-10 提供的像素时钟视觉参考中裁切、按最近邻缩放得到；站立专注猫 PNG 同日由用户提供的完整场景图（最近邻缩放到 230×107，纸张底 #F5F0E3）接入。对应的 C 资源由仓库中的 `managed_components/lvgl__lvgl/scripts/LVGLImage.py` 转为 LVGL RGB565 描述符：它们使用 Flash，不会在 LVGL 内存池中创建同等规模的插画对象。

替换插画时保持上述尺寸，并在构建前重新生成对应的 C 描述符。不要把日期、家庭 Wi-Fi 或其他动态信息烧进源图。

## 专注插画修复（2026-09-12）

布偶、加菲专注图由 imagegen 基于休息图编辑，再缩放为 230x107。这是生成插画，不是真机截图。以当前 PNG 为输入；不要再次对这些图片运行实验脚本 `scratch/build_perfect_assets.py`。

用 `python3 tools/export_pomodoro_assets.py` 导出六张不透明场景，`python3 tools/export_pomodoro_assets.py --check` 校验尺寸及 RGB565 字节（需要 Pillow）。该转换避免透明度乘法压暗不透明颜色。`python3 tools/render_ui_previews.py` 从 C 数组渲染三种皮肤的专注与休息预览。生成的专注房间可能有细微的细节或光照差异；真机显示尚未验收。
