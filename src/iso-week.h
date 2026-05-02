/* ISO 8601 week number from struct tm. Shared by filter and dock. */
#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int iso_week_from_tm(const struct tm *t)
{
	int iso_dow = (t->tm_wday == 0) ? 7 : t->tm_wday;
	int doy = t->tm_yday + 1;
	int thu_ord = doy - iso_dow + 4;
	int year = t->tm_year + 1900;

	if (thu_ord < 1) {
		int py = year - 1;
		int leap = ((py % 4 == 0 && py % 100 != 0) || (py % 400 == 0)) ? 1 : 0;
		thu_ord += leap ? 366 : 365;
	} else {
		int leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 1 : 0;
		int yl = leap ? 366 : 365;
		if (thu_ord > yl)
			thu_ord -= yl;
	}
	return (thu_ord - 1) / 7 + 1;
}

#ifdef __cplusplus
}
#endif
