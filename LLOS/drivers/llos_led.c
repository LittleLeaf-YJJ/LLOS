 /* 
 * 作者: LittleLeaf All rights reserved
 */
#include <llos_led.h>

static uint16_t ledTaskPeriod;
static uint8_t ll_ledNum;

static struct ll_led_config_t *ll_led_config;

static void LLOS_LED_Tick(uint8_t timerN);

void LLOS_LED_Init(uint8_t timerN, uint16_t ms, struct ll_led_config_t *led_config, uint8_t ledNum)
{
	uint32_t size;
	
	if(ms <= 0 || ledNum <= 0 || led_config == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "para NULL!\r\n");
		while(1);
	}
	
	ledTaskPeriod = ms;
	ll_ledNum = ledNum;
	
	size = sizeof(struct ll_led_config_t) * ledNum;
	ll_led_config = LLOS_malloc(size);
	if(ll_led_config == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "led_config malloc null!\r\n");
		while(1);
	}
	memcpy(ll_led_config, led_config, size);
	
	LLOS_Timer_Set(timerN, ll_enable, true, LLOS_Ms_To_Tick(ledTaskPeriod), LLOS_LED_Tick);
}

void LLOS_LED_Set(uint8_t index, enum ll_led_t mode)
{
	if(index >= ll_ledNum || ll_led_config[index].port == 0 || ll_led_config[index].pinMask == 0 || ll_ledNum <= 0 || ledTaskPeriod == 0)
	{
		LL_LOG_E("LLOS_LED_Set ", "index >= ll_ledNum || port == 0 || pinMask == 0 || ll_ledNum <= 0 || ledTaskPeriod == 0!\r\n");
		while(1);
	}
	
	ll_IO_t *temp = (ll_IO_t *)ll_led_config[index].port;

	ll_led_config[index].ledBlink.num = 0; /* 关闭正在闪烁的LED */

	switch(mode)
	{
		case ll_led_off:
		{
			if(ll_led_config[index].isActiveHigh)*temp |= ll_led_config[index].pinMask;
			else *temp &= ~ll_led_config[index].pinMask;
			break;
		}
		case ll_led_on:
		{
			if(ll_led_config[index].isActiveHigh)*temp &= ~ll_led_config[index].pinMask;
			else *temp |= ll_led_config[index].pinMask;
			break;
		}
		case ll_led_toggle:
		{
			*temp ^= ll_led_config[index].pinMask;
			break;
		}
		default:
		{
			if(ll_led_config[index].isActiveHigh)*temp |= ll_led_config[index].pinMask;
			else *temp &= ~ll_led_config[index].pinMask;
			break;
		}
	}
}

void LLOS_LED_Blink(uint8_t index, uint8_t num, uint8_t duty, uint16_t ms)
{
	if(index >= ll_ledNum || ll_led_config[index].port == 0 || ll_led_config[index].pinMask == 0 || ll_ledNum <= 0 || ledTaskPeriod == 0)
	{
		LL_LOG_E("LLOS_LED_Blink ", "index >= ll_ledNum || port == 0 || pinMask == 0 || ll_ledNum <= 0 || ledTaskPeriod == 0!\r\n");
		while(1);
	}
	
	ll_led_config[index].ledBlink.num = num;
	ll_led_config[index].ledBlink.duty = duty;
	ll_led_config[index].ledBlink.ms = ms;
	ll_led_config[index].ledBlink.tick = 0;
}

static void LLOS_LED_Tick(uint8_t timerN)
{
	uint8_t i;
	ll_IO_t *temp;
	
	for(i = 0; i < ll_ledNum; i++)
	{
		if(ll_led_config[i].ledBlink.num == 0)continue;
		
		temp = (ll_IO_t *)ll_led_config[i].port;
		ll_led_config[i].ledBlink.tick++;

		if(ll_led_config[i].ledBlink.tick * ledTaskPeriod * 100 >= ll_led_config[i].ledBlink.ms * ll_led_config[i].ledBlink.duty)
		{
			if(ll_led_config[i].isActiveHigh)*temp |= ll_led_config[i].pinMask;
			else *temp &= ~ll_led_config[i].pinMask;
		}
		else
		{
			if(ll_led_config[i].isActiveHigh)*temp &= ~ll_led_config[i].pinMask;
			else *temp |= ll_led_config[i].pinMask;
		}

		if((ll_led_config[i].ledBlink.tick * ledTaskPeriod) >= ll_led_config[i].ledBlink.ms)
		{
			ll_led_config[i].ledBlink.tick = 0;
			if(ll_led_config[i].ledBlink.num < 255)ll_led_config[i].ledBlink.num--;
		}
	}
}
