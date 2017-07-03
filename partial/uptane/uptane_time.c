#include "uptane_time.h"

bool uptane_time_greater(uptane_time_t l, uptane_time_t r) {
	if(l.year != r.year)
		return l.year > r.year;
	if(l.month != r.month)
		return l.month > r.month;
	if(l.day != r.day)
		return l.day > r.day;
	if(l.hour != r.hour)
		return l.hour > r.hour;
	if(l.minute != r.minute)
		return l.minute > r.minute;
	if(l.second != r.second)
		return l.second > r.second;
	return false;
}
