#include "SKEAZ1284.h"
#include "headlight.h"

#define HEADLIGHT_DIR_REG GPIOA_PDDR
#define HEADLIGHT_SET_REG GPIOA_PSOR
#define HEADLIGHT_CLR_REG GPIOA_PCOR

#define LIGHT0_PIN 16
#define LIGHT1_PIN 17
#define LIGHT2_PIN 18
#define LIGHT3_PIN 19


static const int lights[] = {LIGHT0_PIN, LIGHT1_PIN, LIGHT2_PIN, LIGHT3_PIN};
#define LIGHT_NUM (sizeof(lights)/sizeof(int))

void headlight_init(void)
{
	int i;
	for(i = 0; i < LIGHT_NUM; i++) {
		HEADLIGHT_DIR_REG |= (1 << lights[i]);
		HEADLIGHT_CLR_REG |= (1 << lights[i]);
	}
}

void headlight_set(int num, int val)
{
	if(val) {
		HEADLIGHT_SET_REG = (1 << lights[num]);
	} else {
		HEADLIGHT_CLR_REG = (1 << lights[num]);
  	}
}


