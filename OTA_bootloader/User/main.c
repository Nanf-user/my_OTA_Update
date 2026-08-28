#include "stm32f10x.h"
#include "generic.h"
#include "Serial.h"
#include "LED.h"
#include "W25Q64.h"
#include "ota_config.h"
#include "ota_flash.h"
#include "ota_boot.h"
#include "ota_log.h"

int main(void)
{
	/* Bootloader 运行在 0x08000000, 向量表默认在此, 无需修改 VTOR */

//	delay_init();
	LED_Init();
	Serial_Init();
	W25Q64_Init();

	OtaLog_Str("[BL] Bootloader v1.0 started");
	OtaLog_CRLF();

	{
		uint8_t mid;
		uint16_t did;
		W25Q64_ReadID(&mid, &did);
		OtaLog_Str("[BL] W25Q64 ID: 0x");
		OtaLog_Hex8(mid);
		OtaLog_Hex16(did);
		OtaLog_CRLF();
	}

	OtaBoot_Run();   /* 不返回 */

	while (1)
	{
	}
}
