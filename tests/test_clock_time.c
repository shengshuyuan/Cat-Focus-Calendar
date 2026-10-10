#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "nvs.h"

static time_t wall;
static int64_t saved;
static time_t test_time(time_t *out) {
    if (out) *out = wall;
    return wall;
}
static int test_settimeofday(const struct timeval *tv, const void *tz) {
    (void)tz;
    wall = tv->tv_sec;
    return 0;
}
int nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    (void)ns; (void)mode; *h = 1; return ESP_OK;
}
int nvs_get_i64(nvs_handle_t h, const char *key, int64_t *out) {
    (void)h; (void)key; *out = saved; return saved ? ESP_OK : -1;
}
int nvs_set_i64(nvs_handle_t h, const char *key, int64_t value) {
    (void)h; (void)key; saved = value; return ESP_OK;
}
int nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; }

#define time test_time
#define settimeofday test_settimeofday
#include "../main/clock_time.c"

int main(void) {
    clock_time_bootstrap();
    assert(wall == 0); /* No debug date seeded as current time. */
    assert(clock_time_needs_sync_hint());
    assert(!clock_time_wall_trusted());
    clock_time_on_sntp_sync();
    assert(!clock_time_wall_trusted());

    saved = 1760000000;
    clock_time_bootstrap();
    assert(wall == saved);
    assert(clock_time_trust() == CLOCK_TIME_TRUST_NVS);
    assert(clock_time_needs_sync_hint());
    assert(!clock_time_wall_trusted());

    clock_time_on_sntp_sync();
    assert(clock_time_wall_trusted());
    assert(!clock_time_needs_sync_hint());
    assert(clock_time_take_sync_event());
    assert(!clock_time_take_sync_event());
    wall = 0;
    assert(!clock_time_wall_trusted());
    puts("clock_time: trust regression tests passed");
    return 0;
}
