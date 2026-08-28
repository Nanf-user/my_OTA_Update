#include "ota_flash.h"

/**
  * 函    数：读取参数区
  */
void OtaFlash_ReadParam(ota_param_t *p)
{
	volatile uint32_t *src = (volatile uint32_t *)OTA_PARAM_ADDR;
	uint32_t *dst = (uint32_t *)p;
	uint32_t i, n = sizeof(ota_param_t) / 4;

	for (i = 0; i < n; i++)
	{
		dst[i] = src[i];
	}
}

/**
  * 函    数：写入参数区 (先擦页再按半字编程)
  */
void OtaFlash_WriteParam(ota_param_t *p)
{
	uint16_t *hw = (uint16_t *)p;
	uint32_t i, n = sizeof(ota_param_t) / 2;

	FLASH_Unlock();
	FLASH_ErasePage(OTA_PARAM_ADDR);
	for (i = 0; i < n; i++)
	{
		FLASH_ProgramHalfWord(OTA_PARAM_ADDR + i * 2, hw[i]);
	}
	FLASH_Lock();
}

/**
  * 函    数：清除 OTA 标志 (保留其余字段)
  */
void OtaFlash_ClearFlag(void)
{
	ota_param_t p;

	OtaFlash_ReadParam(&p);
	if (p.magic != OTA_PARAM_MAGIC)
	{
		FLASH_Unlock();
		FLASH_ErasePage(OTA_PARAM_ADDR);
		FLASH_Lock();
		return;
	}
	p.flag = OTA_FLAG_NONE;
	OtaFlash_WriteParam(&p);
}

/**
  * 函    数：擦除 [addr, addr+size) 覆盖的所有 1KB 页
  */
void OtaFlash_EraseRange(uint32_t addr, uint32_t size)
{
	uint32_t start = addr & ~0x3FFUL;
	uint32_t end = (addr + size + 0x3FFUL) & ~0x3FFUL;
	uint32_t a;

	FLASH_Unlock();
	for (a = start; a < end; a += 0x400)
	{
		FLASH_ErasePage(a);
	}
	FLASH_Lock();
}

/**
  * 函    数：按半字编程写入 [addr, addr+size)
  * 注意：addr 与 size 不必对齐, 奇数长度末尾自动补 0xFF
  */
void OtaFlash_WriteRange(uint32_t addr, const uint8_t *data, uint32_t size)
{
	uint32_t i;
	uint16_t hw;

	FLASH_Unlock();
	for (i = 0; i + 1 < size; i += 2)
	{
		hw = (uint16_t)(data[i] | ((uint16_t)data[i + 1] << 8));
		FLASH_ProgramHalfWord(addr + i, hw);
	}
	if (size & 1)
	{
		hw = (uint16_t)(data[size - 1] | 0xFF00);
		FLASH_ProgramHalfWord(addr + size - 1, hw);
	}
	FLASH_Lock();
}

/**
  * 函    数：跳转到 APP (从向量表读取 SP 与复位向量)
  */
void OtaJumpToApp(uint32_t addr)
{
	uint32_t stack = *(volatile uint32_t *)addr;
	uint32_t entry = *(volatile uint32_t *)(addr + 4);
	void (*jump)(void) = (void (*)(void))entry;

	/* 等串口最后一个字节发完, 避免跳转后 SystemInit 重配时钟导致尾部乱码 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{
	}

	/* 复位 SysTick, 避免带病跳转 */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	/* 重定位向量表并设置主堆栈指针 (不清全局中断, APP 启动代码会重新初始化 NVIC) */
	SCB->VTOR = addr;
	__set_MSP(stack);

	jump();
	while (1)
	{
	}
}
