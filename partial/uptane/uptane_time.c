#include "uptane_time.h"

bool uptane_time_greater(uptane_time_t l, uptane_time_t r) {
	return l.year > r.year || l.month > r.month || l.day > r.day ||
	       l.hour > r.hour || l.minute > r.minute || l.second > r.second;
}
