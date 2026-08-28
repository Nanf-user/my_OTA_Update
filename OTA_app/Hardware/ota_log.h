#ifndef __OTA_LOG_H
#define __OTA_LOG_H

#include "stm32f10x.h"

/* 轻量日志输出, 不依赖 printf, 用于减小 Bootloader 体积 */
void OtaLog_Str(const char *s);
void OtaLog_Num(uint32_t v);
void OtaLog_Hex8(uint8_t v);
void OtaLog_Hex16(uint16_t v);
void OtaLog_Hex32(uint32_t v);
void OtaLog_CRLF(void);

#endif
