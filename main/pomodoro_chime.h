#pragma once

#include <stdbool.h>

/* Async ES8311 chime for pomodoro countdown complete. Safe from LVGL timer. */
void pomodoro_chime_notify(bool muted);
