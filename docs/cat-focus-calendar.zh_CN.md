<p align="right">
  <strong>简体中文</strong> · <a href="cat-focus-calendar.md">English</a>
</p>

# 猫猫专注日历 / Cat Focus Calendar — 开发沉淀

> 更新：2026-09-12
> 仓库：本地 `ai-passport`（上游 `FoloToy/ai-passport`）
> 主实现：`main/clock_app.c`、`main/wifi_provision.c`、`main/clock_time.c`、`main/calendar_model.c`

本文档沉淀**当前已定稿**的页面结构、交互、中国日历图层顺序与开发红线，避免后续改 UI 时再次打乱。

更偏上游像素钟说明见 [pixel-clock-v1.zh_CN.md](./pixel-clock-v1.zh_CN.md)。

---

## 1. 产品与硬件

| 项 | 定稿 |
|---|---|
| 产品名 | 猫猫专注日历 / Cat Focus Calendar |
| 芯片 | ESP32-C3，8MB Flash |
| 屏 | 240×320 彩屏 |
| 按键 | 上 / 下 / OK；左侧电源为**硬断电**（固件感知不到点按） |
| Wi‑Fi | 仅 2.4GHz |
| NFC | 板载被动 NTAG213，不接 MCU，固件无 NFC 驱动 |
| 音频 | ES8311，可用于提示音 |
| 电池 | ~500mAh；SOC 来自 CW2017，UI 为粗 3 格 |

---

## 2. 主要页面

共 5 页，入口在万年历：

| 页面 | 代码 | 进入 | 离开 |
|---|---|---|---|
| 万年历 | `PAGE_CALENDAR` / `build_calendar` | 开机默认 | — |
| 常亮壁纸钟 | `PAGE_CLOCK` / `build_clock` | 万年历 **长按 OK** | 短按 OK / 长按 OK 回万年历 |
| 中国日历（撕历） | `PAGE_CHINESE` / `build_chinese` | 万年历 **长按上键** | 短按 OK / 长按 OK |
| 番茄钟 | `PAGE_POMODORO` / `build_pomodoro` | 万年历 **短按 OK** | 短按下键回万年历；长按 OK 也可返回 |
| Wi‑Fi 配网 | `PAGE_WIFI` / `build_wifi` | 万年历 **长按下键** | 长按 OK |

### 2.1 万年历

- 显示年月、月历网格、农历摘要、当月节气名、选中日标记。
- **短按上/下**：翻月（见 §4 翻月规则）。
- 未校时时底部提示：「待校时 长按下键配网」。
- 约 10 分钟无键：只关背光；任意功能键先唤醒，不跳页。


### 2.1a 常亮壁纸钟

- 全屏奶油色猫咪壁纸（`main/assets/folotoy_clock_wallpaper.c`，240×320 RGB565，Flash 常驻；LVGL 直接读 Flash，不另开整帧 RAM）。
- 顶部居中：公历 `YYYY年M月D日`、中文星期、`农历…`（来自**真实今天** `clock_time` 快照，不是万年历浏览选中日）。无「北京时间」。
- 中部大号像素块 `HH:mm`（复用既有 5×7 点阵放大）；墨色约 `#372A22`。
- 时间不可信（从未 NTP/NVS 校时）：显示破折号时钟 +「待校时」；**不用**编译 `__DATE__` 冒充今天。
- 番茄钟在后台继续跑（`tick` 仍调用 `pomodoro_model_tick`）。
- **常亮背光**：本页 `clock_app_idle_backlight_allowed()==false`；`main` 停掉 10 分钟闲置熄屏计时并保持 BL=100；离开后恢复完整 10 分钟闲置。
- 长按 OK 进入时吞掉随后可能的短按 OK，避免误进番茄钟。

### 2.2 中国日历（撕历）— UI 定稿重点

- 大日期数字 + 左右对联 + 山/猫装饰 + 干支 + 宜忌。
- **短按上/下**：前一天 / 后一天。
- 顶栏：`2026年9月` 用 22px，整页居中。星期+农历（`星期一  农历七月廿六`）和节气（`今日白露`）为 13px，同一条中轴。仍**仅当天**显示「今日××」。
- 宜忌各 **2** 条短词（2–4 字）、13px 换行，勿再塞第 3 条。
- 干支框右沿约 `x=152`，猫在 `x=158` 旁侧，不钻进框里。

**图层顺序（底 → 顶）** — 只通过 `stack_chinese_layers()` 维护：

1. 右边对联**框**（边和纸底）
2. 山 `(145,148)` + 猫 `(158,175)`（**坐标勿动**）
3. 太阳
4. 右边对联**文字**「专注当下」，让「当下」和太阳都露出来
5. 大日期数字（scale 12，y=78，在对联井 `x=38 w=164` 内居中；两位数字间距 8px）
6. 顶部：年 / 星期 / 农历 / 节气
7. 干支框「丙午年 …」
8. 宜忌框与文案

**禁止**

- 对右联使用 `lv_obj_move_background()` 沉到整屏最底（会导致对联“消失”）。
- 为躲重叠去挪山/猫坐标；重叠只调层级。

左联「万事顺遂」为同级装饰。右联文字必须在太阳之上。

### 2.3 番茄钟

- 三主题（NVS 保存）：奶油猫咪 / 布偶猫咪 / 加菲猫咪。
- **长按上键**循环主题，约 1.5s toast。
- 场景图约 `230×107` RGB565；纸色/数字色跟主题。**同一皮肤专注/休息共用同一背景色和同一套暖光房间**，只切换猫咪姿势（`scene_focus` 坐姿 / `scene_rest` 睡姿）。
- 待开始时上键轮换时长；OK 开始/暂停/继续；结束可有 ES8311 提示音（尊重静音）。

### 2.4 Wi‑Fi 配网（SoftAP）

- 已从 BLE 配网改为 **SoftAP 网页**：手机连设备热点，浏览器打开 `http://192.168.4.1`。
- 仅 2.4GHz；凭据写入 Flash 并自动重连。
- 连接结果经 `/status` 回传网页；成功后短暂保留 SoftAP 再关掉，再走 NTP。
- 部分网络（如需门户认证）可能 GOT_IP 但 NTP/外网失败 → 屏上可仍「待校时」；可换家庭网或热点验证。

---

## 3. 日期与翻月逻辑

- 选中日 `s_year/s_month/s_day` 在万年历与中国日历间共享。
- **今天锚点** `s_today_*`：开机记住当前展示日；校时成功则刷新。
- **翻月**
  - 翻到非「今天所在月」→ 选中停在 **1 号**（不要把 11 号带到下个月）。
  - 翻回「今天所在月」→ 用锚点还原 **今天**（校时失败也能用开机记住的日期）。

实现：`change_month()` + `remember_today()` / `refresh_today_anchor()`。

---

## 4. 关键代码地图

| 模块 | 路径 |
|---|---|
| 页面 / 按键 / 图层 | `main/clock_app.c`（`stack_chinese_layers`） |
| SoftAP 配网 | `main/wifi_provision.c` |
| 时间 / NVS / 待校时 | `main/clock_time.c` |
| 农历 / 节气 / 干支 | `main/calendar_model.c` + `main/lunar_table.c` |
| 中文字体 | `main/fonts/folotoy_font.c` 与 `folotoy_font_{11,13,22}.c`（`--no-compress`，漏字会空白） |
| 插画 RGB565 | `main/assets/` |

---

## 5. 构建与刷写红线

- 工具链：ESP-IDF **v5.5.3**，目标 esp32c3。
- **默认只写 factory**：`0x10000` ← `build/FoloToy-AI-Passport.bin`。
- **禁止**擦写 `cardid@0x356000`。
- 串口常见：`/dev/cu.usbmodem1101`。
- 刷写前需用户明确确认；先改软件构建验证，再刷。
- Desktop 合并包曾用名：`Cat-Focus-Calendar-full.bin`（发布时再打包更新说明）。

示例（factory-only）：

```bash
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem1101 -b 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x10000 build/FoloToy-AI-Passport.bin
```

---

## 6. 字体与文案

- 增删中文文案必须扩展 `lv_font_conv --symbols`，并加 `--no-compress`。
- 节气「今日」依赖字形「今」；配网文案「在网页」等也曾补字。
- 改完在真机看：中国日历、配网页、番茄主题名。

---

## 7. 改 UI 前自检清单

1. 有没有动 `stack_chinese_layers` 的顺序？若必须动，先出预览再刷。
2. 山/猫坐标是否仍是 `(145,148)` / `(158,175)`？
3. 右联是否仍可见（未被 move_background 沉底）？
4. 干支是否在猫之上？大数字是否在山之上？
5. 节气是否只在当天？宜忌是否仍是 2+2？
6. 翻月：他月 1 号、回本月是否仍是今天？
7. 刷写是否只碰 factory？

---

## 8. 2026-09-11 变更摘要

- SoftAP 配网替代 BLE；Wi‑Fi 凭据 Flash 持久化。
- 中国日历：节气仅当天；农历/节气顶栏居中；宜忌 2+2；图层顺序定稿。
- 翻月选中逻辑用 `s_today_*` 锚点修复「回本月变成 1 号」。
- 番茄三主题、闲置熄屏、校时提示等此前已合入。

---

## 9. 2026-09-12 变更摘要

- 新增常亮壁纸钟页（`PAGE_CLOCK`）：万年历长按 OK 进入，OK 返回；闲置熄屏在本页禁用。
- `clock_time` 区分 NVS 恢复 / 本会话 NTP；壁纸钟仅用受信墙钟。
- 壁纸资源：`assets/images/pixel-clock/clock-wallpaper.png` → `main/assets/folotoy_clock_wallpaper.c`。
