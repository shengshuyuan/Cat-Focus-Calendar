# FoloToy AI Passport Development Notes

<p align="right">
  <a href="FOLOTOY_DEVELOPMENT_NOTES.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

> Scope: ESP32-C3 FoloToy AI Passport firmware development, incremental flashing, and physical-device acceptance in this repository.
> This is an evidence log, not a generic ESP32 tutorial. Append new incidents using the template; never overwrite earlier evidence.

## 1. Board facts that constrain every feature

- ESP32-C3, 8 MB Flash, and **no PSRAM**. The display, LVGL, audio DMA, Wi-Fi/BLE, and task stacks compete for internal RAM.
- ST7789P3 240 × 320 RGB565 display over SPI2. Three buttons share GPIO0 through an ADC resistor ladder; thresholds are authoritative in `components/bsp/include/bsp_pins.h`.
- ES8311 audio codec and CW2017 battery gauge. Do not infer pin, I2C, SPI, or ADC details from a generic ESP32-C3 board.
- `partitions.csv` reserves a protected 16 KB `cardid` NVS partition at `0x356000`. Never erase, move, or overwrite it during application development.
- The repository targets ESP-IDF 5.5.3. On macOS, prefer `$HOME/esp/esp-idf-v5.5.3` and put `/opt/homebrew/bin` first in `PATH` so `idf.py` selects the intended Python environment.

## 2. Engineering rules

1. Read `AGENTS.md`, this directory's hardware guide, and `bsp_pins.h` before implementing a board-facing feature. Update the matching document and record physical results when a mapping changes.
2. Treat memory as a product constraint. Pixel-art screens that create one `lv_obj` per pixel grow quickly; review the LVGL pool, LCD DMA, I2S DMA, task stacks, total free heap, and largest contiguous block together.
3. Access LVGL only in the correct context. LVGL timer callbacks may touch objects directly; button and worker tasks must use `bsp_lvgl_lock()` / `bsp_lvgl_unlock()`. Stop timers/workers before deleting a page.
4. Keep Wi-Fi, NTP, BLE provisioning, audio I/O, and large allocations out of button callbacks and the LVGL task. Return results from worker tasks instead.
5. Provide an offline path. The lunar/solar-term data is generated into a static table; runtime Python and network access are unnecessary for calendar rendering. Network time sync is an enhancement, not a calendar dependency.
6. Report code verification and physical acceptance separately. Static checks, host tests, build, flash, and boot logs do not prove visual layout, button feel, phone provisioning, or ADC behavior across battery levels.
7. Use incremental flashing for provisioned devices. Do not use `erase-flash` or treat a merged image as permission to erase identity data.

## 3. Resolved incidents

### 2026-09-10 — Pixel-art UI exhausted the LVGL pool

- **Symptom**: Switching to the pixel Pomodoro page caused `Guru Meditation Store access fault` while creating `button_glyph` or pixel-number objects, followed by a reboot.
- **Root cause**: Each pixel block was an LVGL object, while the page also kept 42 date labels. The original 24/32 KB pool was too small; with no PSRAM, allocation failure surfaced during object creation.
- **Fix**: Set `CONFIG_LV_MEM_SIZE_KILOBYTES=128`; move the stale generated `sdkconfig` aside and reconfigure rather than assuming `sdkconfig.defaults` changes are active. Keep object counts bounded.
- **Validation**: 64 KB still crashed. The 128 KB build was flashed incrementally and booted repeatedly to `Ready: buttons=1 battery=1` without the reset loop.
- **Remaining risk**: The larger static pool reduces internal RAM available to audio, TLS, radio, and animation buffers. Recheck the largest block and I2S DMA before further UI growth.

### 2026-09-10 — Static illustration should not be a scene graph

- **Symptom**: The first device UI used a small number of large rectangles for the cat, plant, books, mountain, and status art. It was visibly unlike the approved reference and still created many LVGL objects.
- **Root cause**: Fixed visual material was treated as run-time layout rather than as measured pixel artwork. More rectangles would have increased both object count and memory pressure without recovering the reference's detail.
- **Fix**: Crop the approved static artwork into measured 240 × 320-screen assets and convert it into RGB565 LVGL descriptors. `main/assets/` now contains Flash-resident art for the Pomodoro lower scene, calendar mountain, and calendar cat. Time, date, lunar data, selection, battery, Wi-Fi, and button state remain code-driven.
- **Validation**: The firmware builds at `0x1a28b0`, leaving `0x15d750` bytes in the 3 MB app partition. The assets use Flash instead of an equivalent LVGL object graph.
- **Remaining risk**: The screenshot source and the physical panel can differ slightly in paper tone and pixel placement. A physical display photo is still required before claiming visual acceptance.

### 2026-09-10 — Lunar text was a placeholder instead of a calendar model

- **Symptom**: The calendar displayed hard-coded lunar text and a "waiting for network" placeholder; it did not vary by date or expose a real perpetual-calendar detail view.
- **Root cause**: The UI prototype preceded the date model, and runtime network data was treated as mandatory.
- **Fix**: Use `lunar-python` 1.4.8 (MIT) during development to generate a compact C table for 1900-01-01 through 2100-12-31. The firmware links only `main/lunar_table.c/.h` and `main/calendar_model.c/.h`; no Python or third-party API is required at runtime. SNTP only corrects the clock.
- **Boundary evidence**: Host tests cover 2026-09-09 = lunar 7/28, 2024-02-10 = lunar 1/1, 2023-03-22 = leap lunar 2/1, and a solar-term index. Regenerate and rerun these tests whenever the generator changes.
- **Design lesson**: A third-party library is useful as an offline build-time data source, not as a Python runtime dependency on this board. Keep range, license, and generation command in source comments.

### 2026-09-10 — BLE provisioning was initialized too early

- **Symptom**: Boot-time inspection initialized the BLE provisioning manager and then released BTDM using `WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM`; logs showed Bluetooth memory released before a later provisioning session.
- **Root cause**: Reading saved Wi-Fi credentials and starting the BLE provisioning service were coupled.
- **Fix**: At boot, use `esp_wifi_set_mode(WIFI_MODE_STA)` and `esp_wifi_get_config(WIFI_IF_STA)` to inspect the saved SSID. Initialize `wifi_prov_mgr` and BLE only when the user enters provisioning. Development security is `WIFI_PROV_SECURITY_0`; ESP-IDF stores the entered 2.4 GHz SSID/password in Wi-Fi NVS.
- **Network behavior**: On connection, request time from `ntp.aliyun.com` and `pool.ntp.org`; keep local calendar capability offline. Before release, move to per-device PoP with Security 1 and verify retry, wrong-password, repeat-entry, and exit flows on a phone.

### 2026-09-10 — Protected Flash made full erase unsafe

- **Symptom**: A normal ESP32 full erase or mismatched merged image could destroy `cardid` even when the application itself built correctly.
- **Root cause**: This is a fixed factory-app layout, not a dual-slot OTA layout; application, NVS, PHY data, and identity data have fixed addresses.
- **Fix**: `tools/validate.sh --firmware` checks `cardid@0x356000` and the merged image. Daily development uses incremental `idf.py flash`; `idf.py erase-flash` is prohibited.
- **Validation**: The current application is about 1.66 MB, below the 3 MB factory limit; protected-layout and merged-image checks pass.

## 4. Working loop

1. Work in a `feature/*` branch and start with the board documents and source-of-truth files.
2. Run model tests and `tools/validate.sh --static` before building; add `--firmware` when partitions or images are involved.
3. For physical validation, record the firmware revision, flash method, boot log, display/buttons, Wi-Fi/BLE state, and whether device identity data was preserved.
4. After finding a problem, confirm the root cause before recording it; add an automated check or a named manual acceptance step with the fix.

## 5. Incident template

### YYYY-MM-DD — Short title

- **Symptom**:
- **Reproduction**: board, firmware, steps, and frequency.
- **Evidence**: logs, heap data, photos/video, commands, and results.
- **Root cause**: confirmed cause; label hypotheses explicitly.
- **Fix**: code, configuration, hardware, or process change.
- **Validation**: automated checks and physical results separately.
- **Remaining risk / follow-up**: uncovered boards, networks, temperatures, battery levels, or release concerns.

Future incidents will be appended here and reflected in the bilingual index. Historical evidence stays intact so the log records why a fix is trusted, not only that it exists.
