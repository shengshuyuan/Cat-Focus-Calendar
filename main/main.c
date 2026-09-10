#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "clock_app.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "main";

/* Idle backlight: 10 min no keys -> BL 0; any UP/DOWN/OK CLICK/LONG wakes to 100%. */
#define IDLE_BACKLIGHT_MS (10ULL * 60ULL * 1000ULL)

static esp_timer_handle_t s_idle_timer;
static volatile bool s_bl_off;

static void idle_timer_cb(void *arg)
{
    (void)arg;
    s_bl_off = true;
    bsp_display_backlight(0);
    ESP_LOGI(TAG, "Idle timeout: backlight off");
}

static void idle_timer_restart(void)
{
    if (!s_idle_timer) return;
    esp_timer_stop(s_idle_timer);
    esp_err_t err = esp_timer_start_once(s_idle_timer, IDLE_BACKLIGHT_MS * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "idle timer start failed: %s", esp_err_to_name(err));
    }
}

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) {
        return;
    }

    /* Any function key resets idle. If screen was off: wake only, no page jump. */
    if (s_bl_off) {
        s_bl_off = false;
        bsp_display_backlight(100);
        idle_timer_restart();
        ESP_LOGI(TAG, "Key wake: backlight on");
        return;
    }

    idle_timer_restart();

    if (!bsp_lvgl_lock(500)) return;
    clock_app_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "FoloToy pixel calendar + pomodoro boot");

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);
    s_bl_off = false;

    const esp_timer_create_args_t idle_args = {
        .callback = &idle_timer_cb,
        .name = "idle_bl",
    };
    if (esp_timer_create(&idle_args, &s_idle_timer) != ESP_OK) {
        ESP_LOGE(TAG, "idle backlight timer create failed");
    } else {
        idle_timer_restart();
    }

    bool button_ok = bsp_button_init(on_button, NULL) == ESP_OK;
    bool battery_ok = bsp_battery_init() == ESP_OK;
    if (!button_ok) ESP_LOGE(TAG, "Button init failed; UI will be display-only");

    clock_app_prepare(battery_ok);

    if (bsp_lvgl_lock(1000)) {
        clock_app_enter();
        bsp_lvgl_unlock();
    }

    /* fap_screenshot disabled: insufficient contiguous RAM with Wi-Fi */

    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d idle_bl=%llu ms",
             button_ok, battery_ok, (unsigned long long)IDLE_BACKLIGHT_MS);
}
