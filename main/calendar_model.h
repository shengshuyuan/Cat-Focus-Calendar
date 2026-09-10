#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool valid;
    int lunar_year;
    int lunar_month;
    bool leap;
    int lunar_day;
} calendar_lunar_date_t;

int calendar_days_in_month(int year, int month);
int calendar_weekday_sunday_first(int year, int month, int day);
bool calendar_lunar_lookup(int year, int month, int day, calendar_lunar_date_t *out);
bool calendar_month_term(int year, int month, int *day, uint8_t *index);
/* Most recent solar term on or before the given Gregorian date. */
bool calendar_term_on_or_before(int year, int month, int day, uint8_t *index);
const char *calendar_solar_term_name(uint8_t index);
/* Lightweight year/month pillars for tear-off display (立春 / jie month). */
bool calendar_ganzhi_year(int year, int month, int day, char *out, size_t size);
bool calendar_ganzhi_month(int year, int month, int day, char *out, size_t size);
