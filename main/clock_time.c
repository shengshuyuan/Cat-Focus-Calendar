#include "clock_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "clock_time";
#define CLOCK_NVS_NS "clk_time"
#define CLOCK_NVS_KEY "unix"

static bool s_have_saved_time;
static bool s_synced_this_boot;
static volatile bool s_sync_event;

static bool time_looks_valid(const struct tm *local)
{
    return local && (local->tm_year + 1900) >= 2024;
}

static void apply_unix(time_t sec)
{
    struct timeval tv = {.tv_sec = sec, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static void parse_build_date(int *year, int *month, int *day)
{
    static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int d = 1;
    int y = 2026;
    if (sscanf(__DATE__, "%3s %d %d", mon, &d, &y) == 3) {
        const char *p = strstr(kMonths, mon);
        if (p) {
            *month = (int)(p - kMonths) / 3 + 1;
            *day = d;
            *year = y;
            return;
        }
    }
    *year = 2026;
    *month = 9;
    *day = 11;
}

bool clock_time_set_local(int year, int month, int day, int hour, int minute, int second);

void clock_time_bootstrap(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    int64_t saved = 0;
    bool have_nvs = false;
    nvs_handle_t handle;
    if (nvs_open(CLOCK_NVS_NS, NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_i64(handle, CLOCK_NVS_KEY, &saved) == ESP_OK &&
            saved >= 1704067200LL /* 2024-01-01 UTC */) {
            have_nvs = true;
            s_have_saved_time = true;
        }
        nvs_close(handle);
    }

    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    if (!time_looks_valid(&local) && have_nvs) {
        apply_unix((time_t)saved);
        ESP_LOGI(TAG, "restored wall time from NVS (%lld)", (long long)saved);
        now = time(NULL);
        localtime_r(&now, &local);
    } else if (time_looks_valid(&local)) {
        ESP_LOGI(TAG, "RTC already valid (nvs=%d)", (int)have_nvs);
    }

    /* If wall clock is untrusted or stuck before 2026-09-17, seed today. */
    bool need_seed = !time_looks_valid(&local);
    if (!need_seed) {
        int y = local.tm_year + 1900;
        int m = local.tm_mon + 1;
        int d = local.tm_mday;
        if (y < 2026 || (y == 2026 && m < 9) || (y == 2026 && m == 9 && d < 17)) {
            need_seed = true;
        }
    }
    if (need_seed) {
        if (clock_time_set_local(2026, 9, 17, 18, 23, 19)) {
            ESP_LOGI(TAG, "seeded wall clock to 2026-09-17");
        }
    }
}

void clock_time_on_sntp_sync(void)
{
    time_t now = time(NULL);
    if (now < 1704067200LL) return;

    nvs_handle_t handle;
    if (nvs_open(CLOCK_NVS_NS, NVS_READWRITE, &handle) == ESP_OK) {
        if (nvs_set_i64(handle, CLOCK_NVS_KEY, (int64_t)now) == ESP_OK) {
            nvs_commit(handle);
            s_have_saved_time = true;
            ESP_LOGI(TAG, "saved wall time to NVS (%lld)", (long long)now);
        }
        nvs_close(handle);
    }
    s_synced_this_boot = true;
    s_sync_event = true;
}

bool clock_time_needs_sync_hint(void)
{
    /* Hint only when we have never successfully NTP-saved a time. */
    return !s_have_saved_time && !s_synced_this_boot;
}

bool clock_time_take_sync_event(void)
{
    if (!s_sync_event) return false;
    s_sync_event = false;
    return true;
}

bool clock_time_read_today(int *year, int *month, int *day)
{
    if (!year || !month || !day) return false;
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    if (!time_looks_valid(&local)) return false;
    *year = local.tm_year + 1900;
    *month = local.tm_mon + 1;
    *day = local.tm_mday;
    return true;
}

void clock_time_build_date(int *year, int *month, int *day)
{
    parse_build_date(year, month, day);
}


bool clock_time_set_local(int year, int month, int day, int hour, int minute, int second)
{
    if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }

    struct tm local = {0};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    local.tm_isdst = -1;
    time_t sec = mktime(&local);
    if (sec < 1704067200LL) return false;

    apply_unix(sec);
    clock_time_on_sntp_sync();
    ESP_LOGI(TAG, "manual wall clock %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    return true;
}

clock_time_trust_t clock_time_trust(void)
{
    if (s_synced_this_boot) return CLOCK_TIME_TRUST_SESSION;
    if (s_have_saved_time) return CLOCK_TIME_TRUST_NVS;
    return CLOCK_TIME_TRUST_NONE;
}

bool clock_time_wall_trusted(void)
{
    if (!s_have_saved_time && !s_synced_this_boot) return false;
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    return time_looks_valid(&local);
}

bool clock_time_read_local(struct tm *out)
{
    if (!out) return false;
    time_t now = time(NULL);
    localtime_r(&now, out);
    return time_looks_valid(out);
}
