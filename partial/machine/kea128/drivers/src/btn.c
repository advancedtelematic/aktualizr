/* No debouncing is implemented, KBI is not utilized */

#include "SKEAZ1284.h"
#include "btn.h"

#define BTN_OUTE_REG GPIOB_PDDR
#define BTN_INE_REG GPIOB_PIDR
#define BTN_IN_REG GPIOB_PDIR

#define BTN0_PIN 16
#define BTN1_PIN 17
#define BTN2_PIN 18
#define BTN3_PIN 19

static const int btns[] = {BTN0_PIN, BTN1_PIN, BTN2_PIN, BTN3_PIN};
#define BTN_NUM (sizeof(btns)/sizeof(int))

void btn_init(void)
{
	int i;

	for(i = 0; i < BTN_NUM; i++) {
		BTN_OUTE_REG &= ~(1 << btns[i]);
		BTN_INE_REG &= ~(1 << btns[i]);
	}
}

int btn_state(int num)
{
	return !!(BTN_IN_REG & (1 << btns[num]));
}
