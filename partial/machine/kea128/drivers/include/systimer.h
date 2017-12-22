#ifndef ATS_DRIVERS_SYSTIMER_H
#define ATS_DRIVERS_SYSTIMER_H

#include <stdint.h>

extern volatile uint32_t SystemTime;

static inline uint32_t time_get() {return SystemTime;}

void time_init(void);
static inline uint32_t time_passed(uint32_t ts) { return SystemTime - ts; }
void time_delay(uint32_t ms);

#endif
