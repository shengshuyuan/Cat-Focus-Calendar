#include "calendar_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(calendar_days_in_month(2024, 2) == 29);
    assert(calendar_days_in_month(2100, 2) == 28);
    assert(calendar_days_in_month(2026, 9) == 30);
    assert(calendar_days_in_month(2026, 13) == 0);

    /* 2026-09-01 is Tuesday when Sunday is column zero. */
    assert(calendar_weekday_sunday_first(2026, 9, 1) == 2);
    assert(calendar_weekday_sunday_first(2026, 9, 9) == 3);

    calendar_lunar_date_t lunar;
    assert(calendar_lunar_lookup(2026, 9, 9, &lunar));
    assert(lunar.lunar_year == 2026 && lunar.lunar_month == 7 &&
           !lunar.leap && lunar.lunar_day == 28);
    assert(calendar_lunar_lookup(2024, 2, 10, &lunar));
    assert(lunar.lunar_month == 1 && lunar.lunar_day == 1 && !lunar.leap);
    assert(calendar_lunar_lookup(2023, 3, 22, &lunar));
    assert(lunar.lunar_month == 2 && lunar.lunar_day == 1 && lunar.leap);
    int term_day = 0;
    uint8_t term_index = 0;
    assert(calendar_month_term(2026, 9, &term_day, &term_index));
    assert(term_day == 7 && term_index == 17);
    assert(calendar_solar_term_name(term_index)[0] != '\0');

    assert(calendar_term_on_or_before(2026, 9, 10, &term_index));
    assert(term_index == 17); /* 白露 on/after Sep 7 */

    char year_gz[16];
    char month_gz[16];
    assert(calendar_ganzhi_year(2026, 9, 10, year_gz, sizeof(year_gz)));
    assert(strcmp(year_gz, "丙午年") == 0);
    assert(calendar_ganzhi_month(2026, 9, 10, month_gz, sizeof(month_gz)));
    assert(strcmp(month_gz, "丁酉月") == 0);

    /* Before 立春 still previous year pillar. */
    assert(calendar_ganzhi_year(2026, 1, 15, year_gz, sizeof(year_gz)));
    assert(strcmp(year_gz, "乙巳年") == 0);

    puts("calendar_model: all tests passed");
    return 0;
}
