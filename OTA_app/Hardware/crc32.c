#include "crc32.h"

/**
  * 函    数：CRC32 单字节增量计算
  * 参    数：crc  当前 CRC 值 (初始传 0xFFFFFFFF)
  * 参    数：data 数据指针
  * 参    数：len  数据长度
  * 返 回 值：更新后的 CRC 值 (整体算完后再异或 0xFFFFFFFF 即为标准 CRC32)
  */
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
	uint32_t i, b;

	for (i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (b = 0; b < 8; b++)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ 0xEDB88320UL;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

/**
  * 函    数：一次性计算标准 CRC32
  */
uint32_t CRC32_Calc(const uint8_t *data, uint32_t len)
{
	return CRC32_Update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

/**
  * 函    数：一次性计算 CRC16 (Modbus)
  */
uint16_t CRC16_Calc(const uint8_t *data, uint32_t len)
{
	uint16_t crc = 0xFFFF;
	uint32_t i, b;

	for (i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (b = 0; b < 8; b++)
		{
			if (crc & 1)
			{
				crc = (uint16_t)((crc >> 1) ^ 0xA001);
			}
			else
			{
				crc = (uint16_t)(crc >> 1);
			}
		}
	}
	return crc;
}
