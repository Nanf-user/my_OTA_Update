#ifndef __CRC32_H
#define __CRC32_H

#include "stm32f10x.h"

/* 标准 CRC32 (IEEE 802.3, 与 python zlib.crc32 一致) */
uint32_t CRC32_Calc(const uint8_t *data, uint32_t len);

/* 增量 CRC32: init = 0xFFFFFFFF, 最后须异或 0xFFFFFFFF */
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len);

/* CRC16 (Modbus, poly 0x8005 反射, init 0xFFFF) */
uint16_t CRC16_Calc(const uint8_t *data, uint32_t len);

#endif
