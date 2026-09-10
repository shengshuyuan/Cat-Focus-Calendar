#pragma once

#include <stdbool.h>

#include "bsp_button.h"

void clock_app_prepare(bool battery_ok);
void clock_app_enter(void);
void clock_app_back(void);
void clock_app_key(bsp_btn_t btn, bsp_btn_ev_t ev);
