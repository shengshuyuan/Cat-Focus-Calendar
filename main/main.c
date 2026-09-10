#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "clock_app.h"

#include "esp_log.h"

static const char *TAG = "main";

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!bsp_lvgl_lock(500)) return;

    if (ev == BSP_BTN_CLICK || ev == BSP_BTN_LONG) {
        clock_app_key(btn, ev);
    }

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

    bool button_ok = bsp_button_init(on_button, NULL) == ESP_OK;
    bool battery_ok = bsp_battery_init() == ESP_OK;
    if (!button_ok) ESP_LOGE(TAG, "Button init failed; UI will be display-only");
    clock_app_prepare(battery_ok);

    if (bsp_lvgl_lock(1000)) {
        clock_app_enter();
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d", button_ok, battery_ok);
}
