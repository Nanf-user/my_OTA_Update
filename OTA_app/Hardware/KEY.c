#include "stm32f10x.h"
#include "Delay_a.h"

void Key_Init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
		
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_InitStructure.GPIO_Pin =	GPIO_Pin_12 | GPIO_Pin_14  ;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB,&GPIO_InitStructure);

		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
		
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_InitStructure.GPIO_Pin =	GPIO_Pin_0 | GPIO_Pin_3  ;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA,&GPIO_InitStructure);
}

uint8_t Key_GetNum(void)
{
	uint8_t Key_num = 0;
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12) == 0)
	{
		Delay_ms(10);
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12) == 0);
		Delay_ms(10);

		Key_num = 1;
	}
	//
		if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14) == 0)
	{
		Delay_ms(10);
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14) == 0);
		Delay_ms(10);

		Key_num = 2;
	}
	//
		if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_3) == 0)
	{
		Delay_ms(10);
		while(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_3) == 0);
		Delay_ms(10);

		Key_num = 3;
	}
	//
		if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == 0)
	{
		Delay_ms(10);
		while(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == 0);
		Delay_ms(10);

		Key_num = 4;
	}
	return Key_num;
}
