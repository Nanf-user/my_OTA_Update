#ifndef __OTA_FLASH_H
#define __OTA_FLASH_H

#include "stm32f10x.h"
#include "ota_config.h"

/* 参数区读写 */
void OtaFlash_ReadParam(ota_param_t *p);
void OtaFlash_WriteParam(ota_param_t *p);
void OtaFlash_ClearFlag(void);

/* 内部 Flash 擦写 (用于 Bootloader 升级/回退) */
void OtaFlash_EraseRange(uint32_t addr, uint32_t size);
void OtaFlash_WriteRange(uint32_t addr, const uint8_t *data, uint32_t size);

/* 跳转到 APP */
void OtaJumpToApp(uint32_t addr);

#endif
