#include "ota_log.h"
#include "Serial.h"

void OtaLog_Str(const char *s)
{
	Serial_SendString((char *)s);
}

/* 以十进制打印无符号数 (无前导零) */
void OtaLog_Num(uint32_t v)
{
	char tmp[11];
	uint8_t i = 0;

	if (v == 0)
	{
		Serial_SendByte('0');
		return;
	}
	while (v)
	{
		tmp[i++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (i)
	{
		Serial_SendByte(tmp[--i]);
	}
}

void OtaLog_Hex8(uint8_t v)
{
	static const char hex[] = "0123456789ABCDEF";
	Serial_SendByte(hex[(v >> 4) & 0xF]);
	Serial_SendByte(hex[v & 0xF]);
}

void OtaLog_Hex16(uint16_t v)
{
	OtaLog_Hex8((uint8_t)(v >> 8));
	OtaLog_Hex8((uint8_t)(v & 0xFF));
}

void OtaLog_Hex32(uint32_t v)
{
	OtaLog_Hex16((uint16_t)(v >> 16));
	OtaLog_Hex16((uint16_t)(v & 0xFFFF));
}

void OtaLog_CRLF(void)
{
	Serial_SendByte('\r');
	Serial_SendByte('\n');
}
