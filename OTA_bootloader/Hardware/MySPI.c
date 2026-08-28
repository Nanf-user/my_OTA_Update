#include "stm32f10x.h"

/*------------------- SS控制 -------------------*/
void MySPI_W_SS(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_12, (BitAction)BitValue);	//PB12
}

/*------------------- 初始化 -------------------*/
void MySPI_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//GPIOB
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);	//SPI2（注意是APB1！）
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;

	// SS -> PB12（软件控制）
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// SCK + MOSI -> PB13 PB15
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// MISO -> PB14
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*SPI初始化*/
	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_CRCPolynomial = 7;

	SPI_Init(SPI2, &SPI_InitStructure);
	SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Set);
	/*使能SPI2*/
	SPI_Cmd(SPI2, ENABLE);

	/*默认SS高电平*/
	MySPI_W_SS(1);
}

/*------------------- 起始 -------------------*/
void MySPI_Start(void)
{
	MySPI_W_SS(0);
}

/*------------------- 结束 -------------------*/
void MySPI_Stop(void)
{
	MySPI_W_SS(1);
}

/*------------------- 收发 -------------------*/
uint8_t MySPI_SwapByte(uint8_t ByteSend)
{
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) != SET);

	SPI_I2S_SendData(SPI2, ByteSend);

	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) != SET);

	return SPI_I2S_ReceiveData(SPI2);
}
