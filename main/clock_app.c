#include "clock_app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bsp_battery.h"
#include "calendar_model.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "pomodoro_model.h"
#include "pomodoro_store.h"
#include "wifi_provision.h"

extern const lv_font_t folotoy_font;
extern const lv_image_dsc_t folotoy_pomodoro_scene;
extern const lv_image_dsc_t folotoy_pomodoro_scene_focus;
extern const lv_image_dsc_t folotoy_calendar_mountain;
extern const lv_image_dsc_t folotoy_calendar_cat;

#define COLOR_PAPER      0xF5F0E3
#define COLOR_INK        0x17263A
#define COLOR_GREEN      0x506A4D
#define COLOR_GREEN_DARK 0x304B38
#define COLOR_RUST       0xB65B3F
#define COLOR_RED        0xA13127
#define COLOR_SHADOW     0xD8CDB6
#define COLOR_MUTED      0x7C7A70
#define COLOR_SUN        0xEFD8A4
#define COLOR_CLOUD      0xC8C2B4

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef enum {
    PAGE_CALENDAR = 0,
    PAGE_WIFI,
    PAGE_POMODORO,
    PAGE_CHINESE,
} page_t;

typedef struct {
    lv_obj_t *cells[35];
    int x;
    int y;
    int scale;
} pixel_digit_t;

static const char *TAG = "clock_app";
static page_t s_page = PAGE_CALENDAR;
static lv_obj_t *s_scr;
static lv_timer_t *s_timer;
static bool s_battery_ok;
static int s_battery_soc = -1;
static lv_obj_t *s_battery_fill[3];
static pomodoro_model_t s_pomo;
static bool s_prepared;
static bool s_follow_today = true;

/* Shared selected date for month grid and Chinese tear-off page. */
static int s_year = 2026;
static int s_month = 9;
static int s_day = 9;

static lv_obj_t *s_pomo_status;
static lv_obj_t *s_pomo_round;
static lv_obj_t *s_pomo_ok_host;
static lv_obj_t *s_pomo_scene;
static const lv_image_dsc_t *s_pomo_scene_source;
static pixel_digit_t s_pomo_digits[5];
static pixel_digit_t s_cal_year_digits[4];
static pixel_digit_t s_cal_month_digits[2];
static lv_obj_t *s_cal_month_unit;
static lv_obj_t *s_cal_lunar;
static lv_obj_t *s_cal_term;
static lv_obj_t *s_cal_marker;
static lv_obj_t *s_cal_days[42];
static lv_obj_t *s_wifi_status;
static lv_obj_t *s_wifi_name;
static lv_obj_t *s_wifi_hint;

/* Chinese tear-off widgets */
static pixel_digit_t s_cn_day_digits[2];
static lv_obj_t *s_cn_ym;
static lv_obj_t *s_cn_weekday;
static lv_obj_t *s_cn_lunar;
static lv_obj_t *s_cn_term;
static lv_obj_t *s_cn_ganzhi;
static lv_obj_t *s_cn_yi;
static lv_obj_t *s_cn_ji;

static const uint8_t DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

/* Lightweight 宜/忌 lines — not a full almanac; rotate by weekday. */
static const char *const YI_LINES[] = {
    "专注 学习 整理",
    "学习 整理 专注",
    "整理 专注 学习",
    "专注 整理 学习",
    "学习 专注 整理",
    "整理 学习 专注",
    "专注 学习 整理",
};

static const char *const JI_LINES[] = {
    "拖延 熬夜",
    "熬夜 拖延",
    "拖延 熬夜",
    "熬夜 拖延",
    "拖延 熬夜",
    "熬夜 拖延",
    "拖延 熬夜",
};

static lv_obj_t *pixel(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
                      uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *art_image(lv_obj_t *parent, const lv_image_dsc_t *source, int x, int y)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, source);
    lv_obj_set_pos(image, x, y);
    return image;
}

static void status_icons(lv_obj_t *parent)
{
    uint32_t wifi_color = wifi_provision_is_connected() ? COLOR_GREEN : COLOR_INK;
    pixel(parent, 174, 12, 4, 4, wifi_color);
    pixel(parent, 180, 9, 4, 4, wifi_color);
    pixel(parent, 186, 12, 4, 4, wifi_color);
    pixel(parent, 180, 16, 4, 4, wifi_color);
    pixel(parent, 207, 10, 28, 14, COLOR_INK);
    pixel(parent, 211, 13, 20, 8, COLOR_PAPER);
    pixel(parent, 235, 14, 3, 6, COLOR_INK);
    s_battery_fill[0] = pixel(parent, 213, 15, 5, 4, COLOR_GREEN);
    s_battery_fill[1] = pixel(parent, 219, 15, 5, 4, COLOR_GREEN);
    s_battery_fill[2] = pixel(parent, 225, 15, 5, 4, COLOR_GREEN);
}

typedef enum {
    NAV_ICON_UP = 0,
    NAV_ICON_DOWN,
    NAV_ICON_OK,
    NAV_ICON_PLAY,
    NAV_ICON_PAUSE,
    NAV_ICON_DOTS,
} nav_icon_t;

static void clear_children(lv_obj_t *parent)
{
    if (!parent) return;
    while (lv_obj_get_child_count(parent) > 0) {
        lv_obj_delete(lv_obj_get_child(parent, 0));
    }
}

static void paint_nav_icon(lv_obj_t *parent, nav_icon_t icon)
{
    /* Icons are drawn in a 72×28 host; local origin is top-left of the button. */
    clear_children(parent);
    switch (icon) {
        case NAV_ICON_UP:
            pixel(parent, 30, 8, 12, 4, COLOR_PAPER);
            pixel(parent, 33, 5, 6, 4, COLOR_PAPER);
            pixel(parent, 35, 2, 2, 4, COLOR_PAPER);
            pixel(parent, 33, 12, 6, 10, COLOR_PAPER);
            break;
        case NAV_ICON_DOWN:
            pixel(parent, 33, 6, 6, 10, COLOR_PAPER);
            pixel(parent, 30, 16, 12, 4, COLOR_PAPER);
            pixel(parent, 33, 19, 6, 4, COLOR_PAPER);
            pixel(parent, 35, 22, 2, 4, COLOR_PAPER);
            break;
        case NAV_ICON_OK:
            pixel(parent, 24, 6, 24, 16, COLOR_PAPER);
            pixel(parent, 27, 9, 18, 10, COLOR_INK);
            pixel(parent, 30, 13, 4, 4, COLOR_PAPER);
            pixel(parent, 34, 15, 8, 4, COLOR_PAPER);
            pixel(parent, 40, 9, 4, 8, COLOR_PAPER);
            break;
        case NAV_ICON_PLAY:
            pixel(parent, 30, 6, 4, 16, COLOR_PAPER);
            pixel(parent, 34, 8, 4, 12, COLOR_PAPER);
            pixel(parent, 38, 10, 4, 8, COLOR_PAPER);
            pixel(parent, 42, 12, 4, 4, COLOR_PAPER);
            break;
        case NAV_ICON_PAUSE:
            pixel(parent, 28, 6, 6, 16, COLOR_PAPER);
            pixel(parent, 38, 6, 6, 16, COLOR_PAPER);
            break;
        case NAV_ICON_DOTS:
            pixel(parent, 24, 12, 6, 6, COLOR_PAPER);
            pixel(parent, 33, 12, 6, 6, COLOR_PAPER);
            pixel(parent, 42, 12, 6, 6, COLOR_PAPER);
            break;
    }
}

/* Three dark buttons with white icons only — no Chinese labels. */
static lv_obj_t *bottom_nav(lv_obj_t *parent, nav_icon_t left, nav_icon_t mid,
                            nav_icon_t right)
{
    static const int xs[3] = {8, 84, 160};
    const nav_icon_t icons[3] = {left, mid, right};
    lv_obj_t *right_host = NULL;
    pixel(parent, 14, 278, 212, 2, COLOR_SHADOW);
    for (int i = 0; i < 3; i++) {
        pixel(parent, xs[i], 286, 72, 28, COLOR_INK);
        lv_obj_t *host = lv_obj_create(parent);
        lv_obj_remove_flag(host, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(host, xs[i], 286);
        lv_obj_set_size(host, 72, 28);
        lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(host, 0, 0);
        lv_obj_set_style_pad_all(host, 0, 0);
        paint_nav_icon(host, icons[i]);
        if (i == 2) right_host = host;
    }
    return right_host;
}

/* Chinese tear-off only: compact icon + Chinese caption on each button. */
static void paint_nav_icon_compact(lv_obj_t *parent, nav_icon_t icon, int ox)
{
    switch (icon) {
        case NAV_ICON_UP:
            pixel(parent, ox + 4, 10, 8, 3, COLOR_PAPER);
            pixel(parent, ox + 6, 7, 4, 3, COLOR_PAPER);
            pixel(parent, ox + 7, 5, 2, 3, COLOR_PAPER);
            pixel(parent, ox + 6, 13, 4, 8, COLOR_PAPER);
            break;
        case NAV_ICON_DOWN:
            pixel(parent, ox + 6, 7, 4, 8, COLOR_PAPER);
            pixel(parent, ox + 4, 15, 8, 3, COLOR_PAPER);
            pixel(parent, ox + 6, 18, 4, 3, COLOR_PAPER);
            pixel(parent, ox + 7, 20, 2, 3, COLOR_PAPER);
            break;
        case NAV_ICON_OK:
            pixel(parent, ox + 2, 8, 12, 12, COLOR_PAPER);
            pixel(parent, ox + 4, 10, 8, 8, COLOR_INK);
            pixel(parent, ox + 5, 13, 2, 2, COLOR_PAPER);
            pixel(parent, ox + 7, 14, 4, 2, COLOR_PAPER);
            pixel(parent, ox + 10, 10, 2, 5, COLOR_PAPER);
            break;
        default:
            break;
    }
}

static void bottom_nav_zh(lv_obj_t *parent, const char *left, const char *mid,
                          const char *right)
{
    static const int xs[3] = {8, 84, 160};
    const char *labels[3] = {left, mid, right};
    const nav_icon_t icons[3] = {NAV_ICON_UP, NAV_ICON_DOWN, NAV_ICON_OK};
    pixel(parent, 14, 278, 212, 2, COLOR_SHADOW);
    for (int i = 0; i < 3; i++) {
        pixel(parent, xs[i], 286, 72, 28, COLOR_INK);
        lv_obj_t *host = lv_obj_create(parent);
        lv_obj_remove_flag(host, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(host, xs[i], 286);
        lv_obj_set_size(host, 72, 28);
        lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(host, 0, 0);
        lv_obj_set_style_pad_all(host, 0, 0);
        paint_nav_icon_compact(host, icons[i], 2);
        lv_obj_t *label = text(host, labels[i], &folotoy_font, COLOR_PAPER);
        lv_obj_set_pos(label, 18, 4);
        lv_obj_set_size(label, 52, 20);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    }
}

static void calendar_sun(lv_obj_t *parent)
{
    pixel(parent, 207, 70, 8, 2, COLOR_SUN);
    pixel(parent, 203, 72, 16, 3, COLOR_SUN);
    pixel(parent, 201, 75, 20, 12, COLOR_SUN);
    pixel(parent, 203, 87, 16, 3, COLOR_SUN);
    pixel(parent, 207, 90, 8, 2, COLOR_SUN);
}

static void pixel_digit_build(lv_obj_t *parent, pixel_digit_t *digit, int x, int y, int scale)
{
    digit->x = x;
    digit->y = y;
    digit->scale = scale;
    for (int i = 0; i < 35; i++) {
        int row = i / 5;
        int col = i % 5;
        digit->cells[i] = pixel(parent, x + col * scale, y + row * scale,
                                scale, scale, COLOR_INK);
        lv_obj_add_flag(digit->cells[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void pixel_digit_set(pixel_digit_t *digit, int value, uint32_t color)
{
    if (value < 0 || value > 9) value = 0;
    for (int i = 0; i < 35; i++) {
        int row = i / 5;
        int col = i % 5;
        bool on = (DIGITS[value][row] & (1u << (4 - col))) != 0;
        lv_obj_set_style_bg_color(digit->cells[i], lv_color_hex(color), 0);
        if (on) lv_obj_clear_flag(digit->cells[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(digit->cells[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void pixel_digit_blank(pixel_digit_t *digit)
{
    for (int i = 0; i < 35; i++) lv_obj_add_flag(digit->cells[i], LV_OBJ_FLAG_HIDDEN);
}

static void pixel_digit_move(pixel_digit_t *digit, int x, int y)
{
    digit->x = x;
    digit->y = y;
    int scale = digit->scale;
    for (int i = 0; i < 35; i++) {
        int row = i / 5;
        int col = i % 5;
        lv_obj_set_pos(digit->cells[i], x + col * scale, y + row * scale);
    }
}

static void pixel_colon_build(lv_obj_t *parent, pixel_digit_t *digit, int x, int y, int scale)
{
    pixel_digit_build(parent, digit, x, y, scale);
    for (int i = 0; i < 35; i++) lv_obj_add_flag(digit->cells[i], LV_OBJ_FLAG_HIDDEN);
    const int dots[] = {2 * 5 + 2, 4 * 5 + 2};
    for (size_t i = 0; i < ARRAY_SIZE(dots); i++) lv_obj_clear_flag(digit->cells[dots[i]], LV_OBJ_FLAG_HIDDEN);
}

static void reset_screen(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    s_pomo_status = s_pomo_round = s_pomo_ok_host = s_pomo_scene = s_cal_month_unit = s_cal_lunar = s_cal_term = s_cal_marker = NULL;
    s_pomo_scene_source = NULL;
    s_wifi_status = s_wifi_name = s_wifi_hint = NULL;
    s_cn_ym = s_cn_weekday = s_cn_lunar = s_cn_term = s_cn_ganzhi = s_cn_yi = s_cn_ji = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(s_battery_fill); i++) s_battery_fill[i] = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(s_cal_days); i++) s_cal_days[i] = NULL;
    memset(s_pomo_digits, 0, sizeof(s_pomo_digits));
    memset(s_cal_year_digits, 0, sizeof(s_cal_year_digits));
    memset(s_cal_month_digits, 0, sizeof(s_cal_month_digits));
    memset(s_cn_day_digits, 0, sizeof(s_cn_day_digits));
}

static void refresh_battery(void)
{
    if (s_battery_ok) s_battery_soc = bsp_battery_soc();
    int visible = s_battery_soc < 0 ? 0 : (s_battery_soc * 3 + 99) / 100;
    for (int i = 0; i < 3; i++) {
        if (!s_battery_fill[i]) continue;
        if (i < visible) lv_obj_clear_flag(s_battery_fill[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_battery_fill[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void format_lunar(char *out, size_t size, int year, int month, int day)
{
    static const char *const months[] = {"", "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "冬月", "腊月"};
    static const char *const days[] = {"", "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十", "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十", "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};
    calendar_lunar_date_t lunar;
    if (!calendar_lunar_lookup(year, month, day, &lunar)) {
        snprintf(out, size, "农历 待校时");
        return;
    }
    snprintf(out, size, "农历 %s%s", lunar.leap ? "闰" : "", months[lunar.lunar_month]);
    size_t used = strlen(out);
    if (used < size) snprintf(out + used, size - used, "%s", days[lunar.lunar_day]);
}

static void format_term(char *out, size_t size, int year, int month)
{
    int day = 0;
    uint8_t index = 0;
    if (!calendar_month_term(year, month, &day, &index)) {
        snprintf(out, size, "节气 待校时");
        return;
    }
    (void)day;
    snprintf(out, size, "%s", calendar_solar_term_name(index));
}

static void format_term_for_day(char *out, size_t size, int year, int month, int day)
{
    uint8_t index = 0;
    if (!calendar_term_on_or_before(year, month, day, &index)) {
        snprintf(out, size, "节气 待校时");
        return;
    }
    snprintf(out, size, "%s", calendar_solar_term_name(index));
}

static void format_weekday(char *out, size_t size, int year, int month, int day)
{
    static const char *const names[] = {
        "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六",
    };
    int wd = calendar_weekday_sunday_first(year, month, day);
    if (wd < 0 || wd > 6) {
        snprintf(out, size, "星期?");
        return;
    }
    snprintf(out, size, "%s", names[wd]);
}

static void set_calendar_number(pixel_digit_t *digits, int count, int value, uint32_t color)
{
    int divisor = 1;
    for (int i = 1; i < count; i++) divisor *= 10;
    for (int i = 0; i < count; i++) {
        pixel_digit_set(&digits[i], (value / divisor) % 10, color);
        divisor /= 10;
    }
}

static void draw_banner(lv_obj_t *parent, int x, int y, int w, int h, const char *lines)
{
    pixel(parent, x, y, w, h, COLOR_RED);
    pixel(parent, x + 2, y + 2, w - 4, h - 4, COLOR_PAPER);
    pixel(parent, x + 1, y + 1, 3, 3, COLOR_RED);
    pixel(parent, x + w - 4, y + 1, 3, 3, COLOR_RED);
    pixel(parent, x + 1, y + h - 4, 3, 3, COLOR_RED);
    pixel(parent, x + w - 4, y + h - 4, 3, 3, COLOR_RED);
    lv_obj_t *label = text(parent, lines, &folotoy_font, COLOR_RED);
    lv_obj_set_pos(label, x + 2, y + 8);
    lv_obj_set_size(label, w - 4, h - 16);
}

static void draw_rect_frame(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    pixel(parent, x, y, w, 2, color);
    pixel(parent, x, y + h - 2, w, 2, color);
    pixel(parent, x, y, 2, h, color);
    pixel(parent, x + w - 2, y, 2, h, color);
}

static void draw_horse_hint(lv_obj_t *parent, int x, int y)
{
    /* Tiny white horse silhouette for 午-year accent inside large day digit. */
    pixel(parent, x + 6, y + 2, 4, 2, COLOR_PAPER);
    pixel(parent, x + 2, y + 4, 10, 4, COLOR_PAPER);
    pixel(parent, x, y + 6, 4, 4, COLOR_PAPER);
    pixel(parent, x + 10, y + 6, 4, 2, COLOR_PAPER);
    pixel(parent, x + 2, y + 8, 2, 4, COLOR_PAPER);
    pixel(parent, x + 8, y + 8, 2, 4, COLOR_PAPER);
}

static void build_calendar(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    status_icons(s_scr);

    for (int i = 0; i < 4; i++) pixel_digit_build(s_scr, &s_cal_year_digits[i], 13 + i * 21, 12, 4);
    for (int i = 0; i < 2; i++) pixel_digit_build(s_scr, &s_cal_month_digits[i], 14 + i * 42, 51, 8);
    s_cal_month_unit = text(s_scr, "月", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cal_month_unit, 94, 68);
    lv_obj_set_size(s_cal_month_unit, 28, 25);
    s_cal_lunar = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cal_lunar, 119, 47); lv_obj_set_size(s_cal_lunar, 108, 24);
    s_cal_term = text(s_scr, "", &folotoy_font, COLOR_GREEN);
    lv_obj_set_pos(s_cal_term, 119, 73); lv_obj_set_size(s_cal_term, 108, 22);
    art_image(s_scr, &folotoy_calendar_mountain, 116, 76);
    calendar_sun(s_scr);
    art_image(s_scr, &folotoy_calendar_cat, 159, 236);
    lv_obj_move_foreground(s_cal_lunar);
    lv_obj_move_foreground(s_cal_term);

    static const char *WEEKDAYS[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *label = text(s_scr, WEEKDAYS[i], &folotoy_font,
                               (i == 0 || i == 6) ? COLOR_RUST : COLOR_INK);
        lv_obj_set_pos(label, 10 + i * 31, 112); lv_obj_set_size(label, 24, 20);
    }
    s_cal_marker = pixel(s_scr, 0, 0, 25, 25, COLOR_GREEN);
    lv_obj_set_style_radius(s_cal_marker, 4, 0);
    for (int i = 0; i < 42; i++) {
        s_cal_days[i] = text(s_scr, "", &lv_font_montserrat_14, COLOR_INK);
        lv_obj_set_size(s_cal_days[i], 25, 23);
    }
    bottom_nav(s_scr, NAV_ICON_UP, NAV_ICON_DOWN, NAV_ICON_OK);
    lv_screen_load(s_scr);
    refresh_battery();
}

static void refresh_calendar(void)
{
    if (!s_cal_lunar) return;
    char lunar[40];
    char term[40];
    format_lunar(lunar, sizeof(lunar), s_year, s_month, s_day);
    format_term(term, sizeof(term), s_year, s_month);
    set_calendar_number(s_cal_year_digits, 4, s_year, COLOR_INK);
    set_calendar_number(s_cal_month_digits, 2, s_month, COLOR_INK);
    if (s_month < 10) pixel_digit_blank(&s_cal_month_digits[0]);
    lv_label_set_text(s_cal_lunar, lunar);
    lv_label_set_text(s_cal_term, term);

    int first = calendar_weekday_sunday_first(s_year, s_month, 1);
    int total = calendar_days_in_month(s_year, s_month);
    int selected_index = first + (s_day - 1);
    for (int i = 0; i < 42; i++) {
        int day = i - first + 1;
        if (day < 1 || day > total) {
            lv_label_set_text(s_cal_days[i], "");
            continue;
        }
        lv_label_set_text_fmt(s_cal_days[i], "%d", day);
        bool selected = day == s_day;
        lv_obj_set_style_text_color(s_cal_days[i], selected ? lv_color_hex(COLOR_PAPER) :
            ((i % 7 == 0 || i % 7 == 6) ? lv_color_hex(COLOR_RUST) : lv_color_hex(COLOR_INK)), 0);
        lv_obj_set_pos(s_cal_days[i], 10 + (i % 7) * 31, 137 + (i / 7) * 23);
    }
    if (s_day >= 1 && s_day <= total) {
        int x = 10 + (selected_index % 7) * 31;
        int y = 137 + (selected_index / 7) * 23;
        lv_obj_set_pos(s_cal_marker, x, y - 1);
        lv_obj_clear_flag(s_cal_marker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_cal_days[selected_index]);
    } else {
        lv_obj_add_flag(s_cal_marker, LV_OBJ_FLAG_HIDDEN);
    }
}

static void build_chinese(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Soft left clouds */
    pixel(s_scr, 8, 70, 18, 4, COLOR_CLOUD);
    pixel(s_scr, 14, 66, 14, 4, COLOR_CLOUD);
    pixel(s_scr, 10, 96, 16, 4, COLOR_CLOUD);
    pixel(s_scr, 18, 92, 12, 4, COLOR_CLOUD);

    status_icons(s_scr);

    s_cn_ym = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cn_ym, 8, 8);
    lv_obj_set_size(s_cn_ym, 120, 20);
    lv_obj_set_style_text_align(s_cn_ym, LV_TEXT_ALIGN_LEFT, 0);

    s_cn_weekday = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cn_weekday, 8, 28);
    lv_obj_set_size(s_cn_weekday, 100, 20);
    lv_obj_set_style_text_align(s_cn_weekday, LV_TEXT_ALIGN_LEFT, 0);

    /* Keep lunar/term below wifi+battery (icons ~x174-235, y8-24). */
    s_cn_lunar = text(s_scr, "", &folotoy_font, COLOR_GREEN);
    lv_obj_set_pos(s_cn_lunar, 100, 30);
    lv_obj_set_size(s_cn_lunar, 128, 20);
    lv_obj_set_style_text_align(s_cn_lunar, LV_TEXT_ALIGN_RIGHT, 0);

    s_cn_term = text(s_scr, "", &folotoy_font, COLOR_GREEN);
    lv_obj_set_pos(s_cn_term, 118, 50);
    lv_obj_set_size(s_cn_term, 80, 20);
    lv_obj_set_style_text_align(s_cn_term, LV_TEXT_ALIGN_RIGHT, 0);

    draw_banner(s_scr, 10, 54, 28, 110, "万\n事\n顺\n遂");
    draw_banner(s_scr, 202, 54, 28, 110, "专\n注\n当\n下");

    /* Large day digits — positioned in refresh for 1 vs 2 digits. */
    pixel_digit_build(s_scr, &s_cn_day_digits[0], 70, 58, 14);
    pixel_digit_build(s_scr, &s_cn_day_digits[1], 70, 58, 14);
    draw_horse_hint(s_scr, 118, 130);

    art_image(s_scr, &folotoy_calendar_mountain, 145, 148);
    pixel(s_scr, 210, 140, 6, 2, COLOR_SUN);
    pixel(s_scr, 207, 142, 12, 2, COLOR_SUN);
    pixel(s_scr, 205, 144, 16, 8, COLOR_SUN);
    pixel(s_scr, 207, 152, 12, 2, COLOR_SUN);
    pixel(s_scr, 210, 154, 6, 2, COLOR_SUN);
    art_image(s_scr, &folotoy_calendar_cat, 158, 175);

    /* ganzhi frame */
    draw_rect_frame(s_scr, 36, 178, 168, 26, COLOR_RED);
    pixel(s_scr, 32, 186, 6, 10, COLOR_RED);
    pixel(s_scr, 202, 186, 6, 10, COLOR_RED);
    s_cn_ganzhi = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cn_ganzhi, 40, 180);
    lv_obj_set_size(s_cn_ganzhi, 160, 22);

    /* 宜/忌 frame — lightweight copy, not a full almanac. */
    draw_rect_frame(s_scr, 12, 212, 216, 52, COLOR_RED);
    pixel(s_scr, 118, 216, 2, 44, COLOR_RED);

    pixel(s_scr, 20, 222, 22, 22, COLOR_RED);
    lv_obj_t *yi_mark = text(s_scr, "宜", &folotoy_font, COLOR_PAPER);
    lv_obj_set_pos(yi_mark, 20, 223);
    lv_obj_set_size(yi_mark, 22, 22);

    pixel(s_scr, 128, 222, 22, 22, COLOR_GREEN);
    lv_obj_t *ji_mark = text(s_scr, "忌", &folotoy_font, COLOR_PAPER);
    lv_obj_set_pos(ji_mark, 128, 223);
    lv_obj_set_size(ji_mark, 22, 22);

    s_cn_yi = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cn_yi, 46, 224);
    lv_obj_set_size(s_cn_yi, 68, 36);
    lv_obj_set_style_text_align(s_cn_yi, LV_TEXT_ALIGN_LEFT, 0);

    s_cn_ji = text(s_scr, "", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_cn_ji, 154, 224);
    lv_obj_set_size(s_cn_ji, 68, 36);
    lv_obj_set_style_text_align(s_cn_ji, LV_TEXT_ALIGN_LEFT, 0);

    bottom_nav_zh(s_scr, "前一天", "后一天", "返回");
    lv_screen_load(s_scr);
    refresh_battery();
}

static void refresh_chinese(void)
{
    if (!s_cn_ym) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "%d 年 %d 月", s_year, s_month);
    lv_label_set_text(s_cn_ym, buf);

    format_weekday(buf, sizeof(buf), s_year, s_month, s_day);
    lv_label_set_text(s_cn_weekday, buf);

    format_lunar(buf, sizeof(buf), s_year, s_month, s_day);
    lv_label_set_text(s_cn_lunar, buf);

    format_term_for_day(buf, sizeof(buf), s_year, s_month, s_day);
    lv_label_set_text(s_cn_term, buf);

    char year_gz[16];
    char month_gz[16];
    if (calendar_ganzhi_year(s_year, s_month, s_day, year_gz, sizeof(year_gz)) &&
        calendar_ganzhi_month(s_year, s_month, s_day, month_gz, sizeof(month_gz))) {
        snprintf(buf, sizeof(buf), "%s %s", year_gz, month_gz);
    } else {
        snprintf(buf, sizeof(buf), "待校时");
    }
    lv_label_set_text(s_cn_ganzhi, buf);

    int wd = calendar_weekday_sunday_first(s_year, s_month, s_day);
    if (wd < 0 || wd > 6) wd = 0;
    lv_label_set_text(s_cn_yi, YI_LINES[wd]);
    lv_label_set_text(s_cn_ji, JI_LINES[wd]);

    if (s_day < 10) {
        pixel_digit_blank(&s_cn_day_digits[0]);
        pixel_digit_move(&s_cn_day_digits[1], 85, 58);
        pixel_digit_set(&s_cn_day_digits[1], s_day, COLOR_RED);
    } else {
        pixel_digit_move(&s_cn_day_digits[0], 58, 62);
        pixel_digit_move(&s_cn_day_digits[1], 118, 62);
        /* Slightly tighter visual for two digits: keep scale 14 cells. */
        pixel_digit_set(&s_cn_day_digits[0], s_day / 10, COLOR_RED);
        pixel_digit_set(&s_cn_day_digits[1], s_day % 10, COLOR_RED);
    }
}

static void build_wifi(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    status_icons(s_scr);
    pixel(s_scr, 20, 48, 6, 6, COLOR_GREEN);
    pixel(s_scr, 28, 43, 6, 6, COLOR_GREEN);
    pixel(s_scr, 36, 48, 6, 6, COLOR_GREEN);
    lv_obj_t *title = text(s_scr, "Wi-Fi 配网", &folotoy_font, COLOR_GREEN_DARK);
    lv_obj_set_pos(title, 52, 39); lv_obj_set_size(title, 130, 30);
    s_wifi_name = text(s_scr, wifi_provision_service_name(), &lv_font_montserrat_20, COLOR_INK);
    lv_obj_set_pos(s_wifi_name, 22, 93); lv_obj_set_size(s_wifi_name, 196, 30);
    s_wifi_status = text(s_scr, wifi_provision_state_text(), &folotoy_font, COLOR_RUST);
    lv_obj_set_pos(s_wifi_status, 22, 130); lv_obj_set_size(s_wifi_status, 196, 25);
    s_wifi_hint = text(s_scr, "用手机打开 ESP 配网应用", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(s_wifi_hint, 15, 174); lv_obj_set_size(s_wifi_hint, 210, 25);
    lv_obj_t *hint2 = text(s_scr, "搜索上面的设备名", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(hint2, 15, 199); lv_obj_set_size(hint2, 210, 25);
    lv_obj_t *hint3 = text(s_scr, "选择家里 Wi-Fi 并输入密码", &folotoy_font, COLOR_INK);
    lv_obj_set_pos(hint3, 15, 229); lv_obj_set_size(hint3, 210, 25);
    bottom_nav(s_scr, NAV_ICON_UP, NAV_ICON_DOTS, NAV_ICON_PLAY);
    lv_obj_t *back = text(s_scr, "OK开始 上键重试 长按OK返回", &folotoy_font, COLOR_MUTED);
    lv_obj_set_pos(back, 28, 258); lv_obj_set_size(back, 184, 20);
    lv_screen_load(s_scr);
    refresh_battery();
}

static void refresh_wifi(void)
{
    if (!s_wifi_status) return;
    lv_label_set_text(s_wifi_status, wifi_provision_state_text());
    lv_obj_set_style_text_color(s_wifi_status,
                                lv_color_hex(wifi_provision_state() == WIFI_PROVISION_FAILED ? COLOR_RUST : COLOR_GREEN_DARK), 0);
    if (s_wifi_hint) {
        if (wifi_provision_state() == WIFI_PROVISION_CONNECTED) {
            lv_label_set_text(s_wifi_hint, "已连接 时间会自动校准");
        } else if (wifi_provision_state() == WIFI_PROVISION_FAILED) {
            lv_label_set_text(s_wifi_hint, "请确认 2.4G Wi-Fi 后按上重试");
        } else {
            lv_label_set_text(s_wifi_hint, "用手机打开 ESP 配网应用");
        }
    }
}

static void build_pomodoro(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    status_icons(s_scr);

    pixel(s_scr, 16, 18, 12, 5, COLOR_GREEN);
    pixel(s_scr, 20, 12, 5, 10, COLOR_GREEN);
    pixel(s_scr, 25, 20, 5, 5, COLOR_GREEN);
    s_pomo_status = text(s_scr, "专注中", &folotoy_font, COLOR_GREEN_DARK);
    lv_obj_set_pos(s_pomo_status, 53, 12); lv_obj_set_size(s_pomo_status, 96, 26);
    s_pomo_round = text(s_scr, "第 1 / 4 轮", &folotoy_font, COLOR_MUTED);
    lv_obj_set_pos(s_pomo_round, 66, 41); lv_obj_set_size(s_pomo_round, 110, 22);

    int x[] = {12, 57, 102, 117, 162};
    pixel_digit_build(s_scr, &s_pomo_digits[0], x[0], 89, 8);
    pixel_digit_build(s_scr, &s_pomo_digits[1], x[1], 89, 8);
    pixel_colon_build(s_scr, &s_pomo_digits[2], x[2], 89, 8);
    pixel_digit_build(s_scr, &s_pomo_digits[3], x[3], 89, 8);
    pixel_digit_build(s_scr, &s_pomo_digits[4], x[4], 89, 8);

    s_pomo_scene_source = &folotoy_pomodoro_scene;
    s_pomo_scene = art_image(s_scr, s_pomo_scene_source, 5, 168);
    s_pomo_ok_host = bottom_nav(s_scr, NAV_ICON_UP, NAV_ICON_DOWN, NAV_ICON_PLAY);
    lv_screen_load(s_scr);
    refresh_battery();
}

static uint32_t displayed_seconds(void)
{
    switch (s_pomo.state) {
        case POMODORO_FOCUS_RUNNING:
        case POMODORO_FOCUS_PAUSED:
        case POMODORO_ABANDON_CONFIRM: return s_pomo.remaining_sec;
        case POMODORO_BREAK_RUNNING:
        case POMODORO_BREAK_PAUSED: return s_pomo.break_remaining_sec;
        case POMODORO_BREAK_PROMPT: return s_pomo.pending_break_min * 60U;
        default: return pomodoro_model_focus_min(&s_pomo) * 60U;
    }
}

static void refresh_pomodoro(void)
{
    if (!s_pomo_status) return;
    uint32_t seconds = displayed_seconds();
    int values[] = {(int)((seconds / 600) % 10), (int)((seconds / 60) % 10), 0,
                    (int)((seconds / 10) % 6), (int)(seconds % 10)};
    pixel_digit_set(&s_pomo_digits[0], values[0], COLOR_INK);
    pixel_digit_set(&s_pomo_digits[1], values[1], COLOR_INK);
    pixel_digit_set(&s_pomo_digits[3], values[3], COLOR_INK);
    pixel_digit_set(&s_pomo_digits[4], values[4], COLOR_INK);

    const char *status = "准备中";
    uint32_t status_color = COLOR_GREEN_DARK;
    nav_icon_t ok_icon = NAV_ICON_PLAY;

    switch (s_pomo.state) {
        case POMODORO_IDLE:
            status = "准备中";
            ok_icon = NAV_ICON_PLAY;
            break;
        case POMODORO_FOCUS_RUNNING:
            status = "专注中";
            ok_icon = NAV_ICON_PAUSE;
            break;
        case POMODORO_FOCUS_PAUSED:
        case POMODORO_ABANDON_CONFIRM:
            status = "已暂停";
            ok_icon = NAV_ICON_PLAY;
            status_color = COLOR_RUST;
            break;
        case POMODORO_BREAK_RUNNING:
            status = "休息中";
            ok_icon = NAV_ICON_PAUSE;
            break;
        case POMODORO_BREAK_PAUSED:
            status = "休息中";
            ok_icon = NAV_ICON_PLAY;
            break;
        case POMODORO_BREAK_PROMPT:
            status = "休息中";
            ok_icon = NAV_ICON_PLAY;
            break;
        case POMODORO_REWARD:
            status = "完成啦";
            ok_icon = NAV_ICON_PLAY;
            status_color = COLOR_RUST;
            break;
        default:
            status = "专注中";
            ok_icon = NAV_ICON_PLAY;
            break;
    }

    lv_label_set_text(s_pomo_status, status);
    lv_obj_set_style_text_color(s_pomo_status, lv_color_hex(status_color), 0);
    if (s_pomo_ok_host) paint_nav_icon(s_pomo_ok_host, ok_icon);
    const bool focus_pose = (s_pomo.state == POMODORO_FOCUS_RUNNING ||
                             s_pomo.state == POMODORO_FOCUS_PAUSED ||
                             s_pomo.state == POMODORO_ABANDON_CONFIRM);
    const lv_image_dsc_t *scene = focus_pose ? &folotoy_pomodoro_scene_focus
                                             : &folotoy_pomodoro_scene;
    if (s_pomo_scene && s_pomo_scene_source != scene) {
        lv_image_set_src(s_pomo_scene, scene);
        s_pomo_scene_source = scene;
    }
    lv_label_set_text_fmt(s_pomo_round, "第 %u / 4 轮", (unsigned)(s_pomo.pomodoro_round + 1));
}

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void sync_today_from_clock(void)
{
    if (!s_follow_today) return;
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    if (local.tm_year + 1900 < 2024) return;
    int year = local.tm_year + 1900;
    int month = local.tm_mon + 1;
    int day = local.tm_mday;
    if (year != s_year || month != s_month || day != s_day) {
        s_year = year;
        s_month = month;
        s_day = day;
        if (s_page == PAGE_CALENDAR) refresh_calendar();
        else if (s_page == PAGE_CHINESE) refresh_chinese();
    }
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    refresh_battery();
    sync_today_from_clock();
    pomodoro_event_t event = pomodoro_model_tick(&s_pomo, now_ms());
    if (event != POMODORO_EVENT_NONE) pomodoro_store_request_save(&s_pomo);
    if (s_page == PAGE_POMODORO) {
        refresh_pomodoro();
    } else if (s_page == PAGE_WIFI) {
        refresh_wifi();
    }
}

void clock_app_prepare(bool battery_ok)
{
    if (s_prepared) return;
    s_prepared = true;
    s_battery_ok = battery_ok;
    pomodoro_model_defaults(&s_pomo);
    if (!pomodoro_store_init(&s_pomo)) {
        ESP_LOGW(TAG, "Pomodoro state store unavailable; using volatile state");
    }
    wifi_provision_prepare();
    wifi_provision_auto_start();
}

void clock_app_enter(void)
{
    if (!s_prepared) clock_app_prepare(false);
    s_page = PAGE_CALENDAR;
    reset_screen();
    build_calendar();
    refresh_calendar();
    if (!s_timer) s_timer = lv_timer_create(tick, 500, NULL);
}

void clock_app_back(void)
{
    if (s_page == PAGE_WIFI) {
        wifi_provision_stop();
        s_page = PAGE_CALENDAR;
    } else if (s_page == PAGE_POMODORO || s_page == PAGE_CHINESE) {
        s_page = PAGE_CALENDAR;
    } else {
        return;
    }
    reset_screen();
    build_calendar();
    refresh_calendar();
}

static void change_month(int delta)
{
    s_follow_today = false;
    s_month += delta;
    if (s_month < 1) { s_month = 12; s_year--; }
    if (s_month > 12) { s_month = 1; s_year++; }
    int total = calendar_days_in_month(s_year, s_month);
    if (s_day > total) s_day = total;
    if (s_page == PAGE_CALENDAR) refresh_calendar();
}

static void change_day(int delta)
{
    s_follow_today = false;
    s_day += delta;
    while (s_day < 1) {
        s_month--;
        if (s_month < 1) { s_month = 12; s_year--; }
        s_day += calendar_days_in_month(s_year, s_month);
    }
    while (s_day > calendar_days_in_month(s_year, s_month)) {
        s_day -= calendar_days_in_month(s_year, s_month);
        s_month++;
        if (s_month > 12) { s_month = 1; s_year++; }
    }
    if (s_page == PAGE_CHINESE) refresh_chinese();
    else if (s_page == PAGE_CALENDAR) refresh_calendar();
}

void clock_app_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        clock_app_back();
        return;
    }
    if (s_page == PAGE_CALENDAR) {
        if (ev == BSP_BTN_LONG && btn == BSP_BTN_DOWN) {
            s_page = PAGE_WIFI;
            reset_screen();
            build_wifi();
            refresh_wifi();
        } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_UP) {
            s_page = PAGE_CHINESE;
            reset_screen();
            build_chinese();
            refresh_chinese();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            change_month(-1);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            change_month(1);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            s_page = PAGE_POMODORO;
            reset_screen();
            build_pomodoro();
            refresh_pomodoro();
        }
        return;
    }
    if (s_page == PAGE_CHINESE) {
        if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            change_day(-1);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            change_day(1);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            s_page = PAGE_CALENDAR;
            reset_screen();
            build_calendar();
            refresh_calendar();
        }
        return;
    }
    if (s_page == PAGE_WIFI) {
        if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) wifi_provision_start();
        else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP && wifi_provision_state() == WIFI_PROVISION_FAILED) wifi_provision_retry();
        refresh_wifi();
        return;
    }

    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP && s_pomo.state == POMODORO_IDLE) {
        pomodoro_model_select_duration(&s_pomo, 1);
    } else if (btn == BSP_BTN_DOWN) {
        s_page = PAGE_CALENDAR;
        reset_screen();
        build_calendar();
        refresh_calendar();
        return;
    } else if (btn == BSP_BTN_OK) {
        uint64_t current = now_ms();
        if (s_pomo.state == POMODORO_IDLE) {
            pomodoro_model_start_focus(&s_pomo, current);
        } else if (s_pomo.state == POMODORO_FOCUS_RUNNING || s_pomo.state == POMODORO_BREAK_RUNNING) {
            pomodoro_model_pause(&s_pomo, current);
        } else if (s_pomo.state == POMODORO_FOCUS_PAUSED || s_pomo.state == POMODORO_BREAK_PAUSED) {
            pomodoro_model_resume(&s_pomo, current);
        } else if (s_pomo.state == POMODORO_BREAK_PROMPT) {
            pomodoro_model_start_break(&s_pomo, current);
        }
    }
    pomodoro_store_request_save(&s_pomo);
    refresh_pomodoro();
}
