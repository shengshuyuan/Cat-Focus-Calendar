#include "calendar_model.h"

#include <stdio.h>
#include <stddef.h>

#include "lunar_table.h"

static const int DAYS_IN_MONTH[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

static const char *const STEMS[] = {
    "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸",
};

static const char *const BRANCHES[] = {
    "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥",
};

int calendar_days_in_month(int year, int month)
{
    if (month < 1 || month > 12) return 0;
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return DAYS_IN_MONTH[month - 1];
}

int calendar_weekday_sunday_first(int year, int month, int day)
{
    if (month < 3) { month += 12; year--; }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7;
}

static bool valid_date(int year, int month, int day)
{
    return year >= 1900 && year <= 2100 && month >= 1 && month <= 12 &&
           day >= 1 && day <= calendar_days_in_month(year, month);
}

static uint32_t solar_ordinal(int year, int month, int day)
{
    int y = year;
    int m = month;
    if (m <= 2) {
        y--;
        m += 12;
    }
    int days = 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + day - 1;
    int base_y = 1899;
    int base = 365 * base_y + base_y / 4 - base_y / 100 + base_y / 400 + (153 * 10 + 2) / 5;
    return (uint32_t)(days - base);
}

static void ordinal_to_date(uint32_t ordinal, int *year, int *month, int *day)
{
    int remaining = ordinal;
    int y = 1900;
    while (remaining >= (calendar_days_in_month(y, 1) + calendar_days_in_month(y, 2) +
                         calendar_days_in_month(y, 3) + calendar_days_in_month(y, 4) +
                         calendar_days_in_month(y, 5) + calendar_days_in_month(y, 6) +
                         calendar_days_in_month(y, 7) + calendar_days_in_month(y, 8) +
                         calendar_days_in_month(y, 9) + calendar_days_in_month(y, 10) +
                         calendar_days_in_month(y, 11) + calendar_days_in_month(y, 12))) {
        remaining -= 365 + ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        y++;
    }
    int m = 1;
    while (remaining >= calendar_days_in_month(y, m)) {
        remaining -= calendar_days_in_month(y, m++);
    }
    *year = y;
    *month = m;
    *day = remaining + 1;
}

bool calendar_lunar_lookup(int year, int month, int day, calendar_lunar_date_t *out)
{
    if (!out) return false;
    *out = (calendar_lunar_date_t){0};
    if (!valid_date(year, month, day)) return false;
    uint32_t target = solar_ordinal(year, month, day);
    size_t low = 0;
    size_t high = lunar_month_table_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (lunar_month_table[mid].solar_day <= target) low = mid + 1;
        else high = mid;
    }
    if (low == 0) return false;
    const lunar_month_record_t *record = &lunar_month_table[low - 1];
    int lunar_day = (int)(target - record->solar_day) + 1;
    if (lunar_day > record->lunar_days) return false;
    out->valid = true;
    out->lunar_year = record->lunar_year;
    out->leap = record->lunar_month < 0;
    out->lunar_month = record->lunar_month < 0 ? -record->lunar_month : record->lunar_month;
    out->lunar_day = lunar_day;
    return true;
}

bool calendar_month_term(int year, int month, int *day, uint8_t *index)
{
    if (day) *day = 0;
    if (index) *index = 0;
    if (year < 1900 || year > 2100 || month < 1 || month > 12) return false;
    for (size_t i = 0; i < lunar_term_table_count; i++) {
        int term_year;
        int term_month;
        int term_day;
        ordinal_to_date(lunar_term_table[i].solar_day, &term_year, &term_month, &term_day);
        if (term_year == year && term_month == month) {
            if (day) *day = term_day;
            if (index) *index = lunar_term_table[i].term_index;
            return true;
        }
    }
    return false;
}

bool calendar_term_on_or_before(int year, int month, int day, uint8_t *index)
{
    if (index) *index = 0;
    if (!valid_date(year, month, day)) return false;
    uint32_t target = solar_ordinal(year, month, day);
    size_t low = 0;
    size_t high = lunar_term_table_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (lunar_term_table[mid].solar_day <= target) low = mid + 1;
        else high = mid;
    }
    if (low == 0) return false;
    if (index) *index = lunar_term_table[low - 1].term_index;
    return true;
}

const char *calendar_solar_term_name(uint8_t index)
{
    static const char *const names[] = {
        "冬至", "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明",
        "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋",
        "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪",
    };
    return index < (sizeof(names) / sizeof(names[0])) ? names[index] : "";
}

/* Odd term indices are the 12 jie that open a ganzhi month. */
static bool term_is_jie(uint8_t term_index)
{
    return (term_index % 2u) == 1u;
}

/* 小寒→丑(12), 立春→寅(1), 惊蛰→卯(2), ... 大雪→子(11). */
static int jie_month_number(uint8_t term_index)
{
    if (term_index == 1) return 12;
    return (int)((term_index - 3) / 2) + 1;
}

static bool find_jie_month(int year, int month, int day, int *month_num)
{
    if (!valid_date(year, month, day) || !month_num) return false;
    uint32_t target = solar_ordinal(year, month, day);
    size_t low = 0;
    size_t high = lunar_term_table_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (lunar_term_table[mid].solar_day <= target) low = mid + 1;
        else high = mid;
    }
    for (size_t i = low; i > 0; i--) {
        uint8_t idx = lunar_term_table[i - 1].term_index;
        if (term_is_jie(idx)) {
            *month_num = jie_month_number(idx);
            return true;
        }
    }
    return false;
}

static int chinese_year_number(int year, int month, int day)
{
    /* Traditional year pillar flips at 立春 (term index 3). */
    uint8_t idx = 0;
    int lichun_day = 0;
    int lichun_month = 0;
    for (size_t i = 0; i < lunar_term_table_count; i++) {
        int ty, tm, td;
        ordinal_to_date(lunar_term_table[i].solar_day, &ty, &tm, &td);
        if (ty == year && lunar_term_table[i].term_index == 3) {
            lichun_month = tm;
            lichun_day = td;
            idx = 3;
            break;
        }
    }
    (void)idx;
    if (lichun_month != 0) {
        if (month < lichun_month || (month == lichun_month && day < lichun_day)) {
            return year - 1;
        }
    }
    return year;
}

bool calendar_ganzhi_year(int year, int month, int day, char *out, size_t size)
{
    if (!out || size == 0) return false;
    out[0] = '\0';
    if (!valid_date(year, month, day)) return false;
    int cy = chinese_year_number(year, month, day);
    int stem = (cy - 4) % 10;
    int branch = (cy - 4) % 12;
    if (stem < 0) stem += 10;
    if (branch < 0) branch += 12;
    snprintf(out, size, "%s%s年", STEMS[stem], BRANCHES[branch]);
    return true;
}

bool calendar_ganzhi_month(int year, int month, int day, char *out, size_t size)
{
    if (!out || size == 0) return false;
    out[0] = '\0';
    int month_num = 0;
    if (!find_jie_month(year, month, day, &month_num)) return false;
    int cy = chinese_year_number(year, month, day);
    int year_stem = (cy - 4) % 10;
    if (year_stem < 0) year_stem += 10;
    /* 五虎遁: 甲己丙、乙庚戊、丙辛庚、丁壬壬、戊癸甲. */
    static const int YIN_STEM[] = {2, 4, 6, 8, 0};
    int yin_stem = YIN_STEM[year_stem % 5];
    int month_stem = (yin_stem + month_num - 1) % 10;
    /* 寅=2 ... 丑=1 */
    int month_branch = (month_num + 1) % 12;
    snprintf(out, size, "%s%s月", STEMS[month_stem], BRANCHES[month_branch]);
    return true;
}
