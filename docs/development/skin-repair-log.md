<p align="right"><a href="skin-repair-log.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Skin repair log

## 2026-09-12 — Focus artwork and countdown boundary

- Symptoms/reproduction: select ragdoll or Exotic-shorthair, start focus. Both focus PNGs still contained the sleeping cat; Exotic-shorthair focus also had displaced fragments across the face and right-hand props.
- Evidence/root cause: `scratch/build_perfect_assets.py` starts focus from the rest frame, then copies only x=140..219, y=15..94 from the focus frame using color thresholds. The cat mainly lies left of that region. This retains the sleeping cat while copying unrelated room fragments. The state-to-scene mapping in `clock_app.c` already selects the focus descriptor correctly.
- Repair: edit the two full focus scenes from their rest references; resize to 230x107 and regenerate RGB565. Do not rerun the experimental script on the repaired inputs. The new `tools/export_pomodoro_assets.py --check` verifies all six PNG/C pairs, dimensions and stride. Direct opaque quantization also avoids the previous converter's 255/256 alpha darkening.
- Additional confirmed bug: pausing focus or break exactly at the deadline left a zero-second paused state that could neither resume nor complete. Keep that state running until the normal tick delivers the completion event. Regression tests cover both phases.
- Documentation: align the old paused-pose description with the implementation: focus pause retains the awake scene; idle/break use rest. Abandon and skip-break model APIs have no UI entry; this audit does not add new controls.
- Validation: host tests PASS; six RGB565 asset checks PASS; inspected 240x320 previews rendered from the firmware arrays. These are source renders, not LVGL/device captures.
- Physical acceptance: NOT RUN; no device flash performed. Check starting/pausing/resuming and theme switching on the device, plus the final-second pause boundary. Generated focus rooms retain the composition but can differ slightly in lighting/details from rest; exact background pixel identity is not claimed.

- Build result: complete `tools/validate.sh` gate PASS with ESP-IDF v5.5.3 using the existing Python 3.14 environment. Application 1,945,872 / 3,145,728 bytes; merged-image and protected-layout checks PASS. Verified merged output: `build/FoloToy-AI-Passport-full.bin`. No flash performed.
