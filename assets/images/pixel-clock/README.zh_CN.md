<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 像素时钟插画资源

这些 PNG 保存 240 × 320 像素时钟页面中的固定像素插画。倒计时、番茄钟状态、农历、选中日期、电量和 Wi-Fi 状态仍由 LVGL 代码实时绘制。

| 源图 | 尺寸 | 固件输出 | 放置位置 |
| --- | --- | --- | --- |
| `pomodoro-scene.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene.c` | IDLE / BREAK_* / BREAK_PROMPT / REWARD 等休息或结束后的番茄钟下半部：绿植、趴姿猫、杯子和书。 |
| `pomodoro-scene-focus.png` | 230 × 107 RGB | `main/assets/folotoy_pomodoro_scene_focus.c` | FOCUS_RUNNING / FOCUS_PAUSED / ABANDON_CONFIRM 专注相关状态的番茄钟下半部：绿植、站立三花猫、FOCUS 书与杯子。 |
| `calendar-mountain.png` | 121 × 28 RGB | `main/assets/folotoy_calendar_mountain.c` | 万年历右上角的山景与枝叶。 |
| `calendar-cat.png` | 76 × 32 RGB | `main/assets/folotoy_calendar_cat.c` | 万年历右下角的猫和枝叶。 |

原始睡猫 PNG 从用户在 2026-09-10 提供的像素时钟视觉参考中裁切、按最近邻缩放得到；站立专注猫 PNG 同日由用户提供的完整场景图（最近邻缩放到 230×107，纸张底 #F5F0E3）接入。对应的 C 资源由仓库中的 `managed_components/lvgl__lvgl/scripts/LVGLImage.py` 转为 LVGL RGB565 描述符：它们使用 Flash，不会在 LVGL 内存池中创建同等规模的插画对象。

替换插画时保持上述尺寸，并在构建前重新生成对应的 C 描述符。不要把日期、家庭 Wi-Fi 或其他动态信息烧进源图。
