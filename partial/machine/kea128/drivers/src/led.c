#include "SKEAZ1284.h"
#include "led.h"

#define LED_DIR_REG GPIOA_PDDR
#define LED_SET_REG GPIOA_PSOR
#define LED_CLR_REG GPIOA_PCOR

#define LED0_PIN 10
#define LED1_PIN 11
#define LED2_PIN 12
#define LED3_PIN 13

static const int leds[] = {LED0_PIN, LED1_PIN, LED2_PIN, LED3_PIN};
#define LED_NUM (sizeof(leds)/sizeof(int))

void led_init(void)
{
	int i;

	// disable external NMI on the pin used for LED2
	SIM->SOPT0 &= ~(0x00000002);
	for(i = 0; i < LED_NUM; i++) {
		LED_DIR_REG |= (1 << leds[i]);
		LED_CLR_REG = (1 << leds[i]);
	}
}

void led_set(int num, int val)
{
	if(val)
		LED_SET_REG = (1 << leds[num]);
	else
		LED_CLR_REG = (1 << leds[num]);
}
