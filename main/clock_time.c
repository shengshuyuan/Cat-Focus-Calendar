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

void clock_time_bootstrap(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    if (time_looks_valid(&local)) {
        ESP_LOGI(TAG, "RTC already valid");
        return;
    }

    nvs_handle_t handle;
    if (nvs_open(CLOCK_NVS_NS, NVS_READONLY, &handle) == ESP_OK) {
        int64_t saved = 0;
        if (nvs_get_i64(handle, CLOCK_NVS_KEY, &saved) == ESP_OK &&
            saved >= 1704067200LL /* 2024-01-01 UTC */) {
            apply_unix((time_t)saved);
            s_have_saved_time = true;
            ESP_LOGI(TAG, "restored wall time from NVS (%lld)", (long long)saved);
        }
        nvs_close(handle);
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
