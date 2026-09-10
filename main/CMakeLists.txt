idf_component_register(
    SRCS "main.c"
         "clock_app.c"
         "calendar_model.c"
         "lunar_table.c"
         "pomodoro_model.c"
         "pomodoro_store.c"
         "wifi_provision.c"
         "fonts/folotoy_font.c"
         "assets/folotoy_pomodoro_scene.c"
         "assets/folotoy_pomodoro_scene_focus.c"
         "assets/folotoy_calendar_mountain.c"
         "assets/folotoy_calendar_cat.c"
         "fap_screenshot.c"
    INCLUDE_DIRS "."
    REQUIRES bsp driver esp_timer nvs_flash esp_event esp_netif esp_wifi bt wifi_provisioning
)
