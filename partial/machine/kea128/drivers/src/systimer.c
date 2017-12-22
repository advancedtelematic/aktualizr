#include "systimer.h"
#include "SKEAZ1284.h"

volatile uint32_t SystemTime = 0;

void time_init(void)
{
	SysTick_Config(SystemCoreClock/1000); // once in a millisecond
}

void time_delay(uint32_t ms)
{
	uint32_t ts = time_get();
	while(time_passed(ts) < ms);
}

void SysTick_Handler(void)
{
	SystemTime++;
}

