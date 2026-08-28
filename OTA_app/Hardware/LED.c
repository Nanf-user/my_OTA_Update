#include "stm32f10x.h"                  // Device 
#include "LED.h"
void LED_Init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
		//		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin =	GPIO_Pin_8  ;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA,&GPIO_InitStructure);
		
		GPIO_SetBits(GPIOA,GPIO_Pin_8 );
}

void LED_1_On(void)
{
		GPIO_ResetBits(GPIOA,GPIO_Pin_8);

}

void LED_1_Off(void)
{
		GPIO_SetBits(GPIOA,GPIO_Pin_8);

}
//void LED_2_On(void)
//{
//		GPIO_ResetBits(GPIOA,GPIO_Pin_7);

//}
//void LED_2_Off(void)
//{
//		GPIO_SetBits(GPIOA,GPIO_Pin_7);

//}
//void LED_2_Turn(void)
//{
//	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_7) == 0)
//	{
//		GPIO_SetBits(GPIOA,GPIO_Pin_7);
//	}
//	else
//	{
//		GPIO_ResetBits(GPIOA,GPIO_Pin_7);
//	}

//}
//void LED_1_Turn(void)
//{
//	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_6) == 0)
//	{
//		GPIO_SetBits(GPIOA,GPIO_Pin_6);
//	}
//	else
//	{
//		GPIO_ResetBits(GPIOA,GPIO_Pin_6);
//	}

//}

