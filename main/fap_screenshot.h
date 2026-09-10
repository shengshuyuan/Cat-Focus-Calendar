#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* 本应用内存不足以同时保留 Wi-Fi 与整屏截屏缓冲;协议暂禁用。 */
static inline void fap_screenshot_start(void) {}
#ifdef __cplusplus
}
#endif
