#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t solar_day;
    int16_t lunar_year;
    int8_t lunar_month;
    uint8_t lunar_days;
} lunar_month_record_t;

typedef struct {
    uint32_t solar_day;
    uint8_t term_index;
} lunar_term_record_t;

extern const lunar_month_record_t lunar_month_table[];
extern const size_t lunar_month_table_count;
extern const lunar_term_record_t lunar_term_table[];
extern const size_t lunar_term_table_count;
