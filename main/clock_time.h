#pragma once

#include <stdbool.h>
#include <time.h>

/** TZ + restore last NTP time from NVS when RTC invalid. */
void clock_time_bootstrap(void);

/** Compile-time __DATE__ as y/m/d fallback before RTC/NVS is usable. */
void clock_time_build_date(int *year, int *month, int *day);

/** Persist current wall clock after SNTP succeeds. */
void clock_time_on_sntp_sync(void);

/** True until we have ever saved an NTP time (NVS) or synced this boot. */
bool clock_time_needs_sync_hint(void);

/** Consume one-shot "time just synced" edge for UI refresh. */
bool clock_time_take_sync_event(void);

/** Fill y/m/d from localtime when valid (>=2024); otherwise false. */
bool clock_time_read_today(int *year, int *month, int *day);

/**
 * Wall time is trusted for the always-on clock page when we have an NVS-saved
 * NTP sample and/or a session sync, and localtime looks valid. Compile __DATE__
 * alone is never trusted.
 */
bool clock_time_wall_trusted(void);

/** One time() snapshot into local broken-down time; false if year < 2024. */
bool clock_time_read_local(struct tm *out);

/** Trust provenance for diagnostics / UI. */
typedef enum {
    CLOCK_TIME_TRUST_NONE = 0,
    CLOCK_TIME_TRUST_NVS,
    CLOCK_TIME_TRUST_SESSION,
} clock_time_trust_t;

clock_time_trust_t clock_time_trust(void);
