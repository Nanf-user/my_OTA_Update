#include "ota_app.h"
#include "ota_config.h"
#include "ota_uart.h"
#include "ota_log.h"
#include "ota_flash.h"
#include "crc32.h"
#include "W25Q64.h"
#include "generic.h"
#include <string.h>

typedef enum {
	OTA_APP_IDLE = 0,      /* 空闲, 等待 START */
	OTA_APP_RECEIVING = 1  /* 接收中 */
} ota_app_state_t;

static ota_app_state_t s_state = OTA_APP_IDLE;
static uint32_t s_expected_seq = 0;
static uint32_t s_total_size = 0;
static uint32_t s_expected_crc32 = 0;
static uint32_t s_new_version = 0;
static uint8_t  s_buf[OTA_PKT_DATA_SIZE];

/* 擦除下载区 (按 4KB 扇区) */
static void EraseDownloadArea(uint32_t size)
{
	uint32_t off;
	for (off = 0; off < size; off += 4096)
	{
		W25Q64_SectorErase(OTA_EXT_DL_ADDR + off);
	}
}

/* 备份当前 APP 到 W25Q64 备份区, 用于升级失败时回退 */
static uint8_t BackupCurrentApp(void)
{
	uint32_t off;

	OtaLog_Str("[APP] Backup current APP to W25Q64...");
	OtaLog_CRLF();

	for (off = 0; off < OTA_APP_MAX_SIZE; off += 4096)
	{
		W25Q64_SectorErase(OTA_EXT_BACKUP_ADDR + off);
	}
	for (off = 0; off < OTA_APP_MAX_SIZE; off += OTA_PKT_DATA_SIZE)
	{
		memcpy(s_buf, (const void *)(OTA_APP_ADDR + off), OTA_PKT_DATA_SIZE);
		W25Q64_PageProgram(OTA_EXT_BACKUP_ADDR + off, s_buf, OTA_PKT_DATA_SIZE);
	}
	return 1;
}

/* 校验下载区 CRC32 */
static uint8_t VerifyDownloadArea(void)
{
	uint32_t crc = 0xFFFFFFFFUL, off, n;

	OtaLog_Str("[APP] Verify CRC32...");
	OtaLog_CRLF();

	for (off = 0; off < s_total_size; off += OTA_PKT_DATA_SIZE)
	{
		n = (s_total_size - off) > OTA_PKT_DATA_SIZE ? OTA_PKT_DATA_SIZE : (s_total_size - off);
		W25Q64_ReadData(OTA_EXT_DL_ADDR + off, s_buf, n);
		crc = CRC32_Update(crc, s_buf, n);
	}
	crc ^= 0xFFFFFFFFUL;

	OtaLog_Str("[APP] Calc=0x"); OtaLog_Hex32(crc);
	OtaLog_Str(" Expect=0x"); OtaLog_Hex32(s_expected_crc32);
	OtaLog_CRLF();

	return (crc == s_expected_crc32) ? 1 : 0;
}

void OtaApp_Init(void)
{
	s_state = OTA_APP_IDLE;
	s_expected_seq = 0;
	s_total_size = 0;
	s_expected_crc32 = 0;
	s_new_version = 0;
}

/**
  * 函    数：OTA 接收状态机, 由主循环轮询
  * 说    明：收到 START 后进入接收态; 校验通过则备份 + 写标志 + 软复位
  */
void OtaApp_Process(void)
{
	ota_frame_t frame;
	uint32_t size, crc;
	uint16_t vmaj, vmin;

	if (!OtaUart_Poll(&frame))
	{
		return;
	}

	switch (s_state)
	{
	case OTA_APP_IDLE:
		if (frame.cmd != OTA_CMD_START)
		{
			return;
		}
		if (frame.len < 12)
		{
			OtaUart_SendResponse(OTA_RESP_START_ERR, 0);
			return;
		}
		size = ((uint32_t)frame.payload[0] << 24) | ((uint32_t)frame.payload[1] << 16) |
		       ((uint32_t)frame.payload[2] << 8) | frame.payload[3];
		crc  = ((uint32_t)frame.payload[4] << 24) | ((uint32_t)frame.payload[5] << 16) |
		       ((uint32_t)frame.payload[6] << 8) | frame.payload[7];
		vmaj = (uint16_t)((frame.payload[8] << 8) | frame.payload[9]);
		vmin = (uint16_t)((frame.payload[10] << 8) | frame.payload[11]);

		if (size == 0 || size > OTA_APP_MAX_SIZE)
		{
			OtaUart_SendResponse(OTA_RESP_START_ERR, 0);
			return;
		}

		s_total_size = size;
		s_expected_crc32 = crc;
		s_new_version = ((uint32_t)vmaj << 16) | vmin;
		s_expected_seq = 0;

		EraseDownloadArea(size);
		s_state = OTA_APP_RECEIVING;

		OtaLog_Str("[APP] OTA START: size="); OtaLog_Num(size);
		OtaLog_Str(" ver="); OtaLog_Num(vmaj); OtaLog_Str("."); OtaLog_Num(vmin);
		OtaLog_CRLF();

		OtaUart_SendResponse(OTA_RESP_START_OK, 0);
		break;

	case OTA_APP_RECEIVING:
		if (frame.cmd == OTA_CMD_DATA)
		{
			if (frame.seq == s_expected_seq)
			{
				if (frame.len > OTA_PKT_DATA_SIZE)
				{
					OtaUart_SendResponse(OTA_NAK, (uint16_t)s_expected_seq);
					return;
				}
				W25Q64_PageProgram(OTA_EXT_DL_ADDR + (uint32_t)frame.seq * OTA_PKT_DATA_SIZE,
				                   frame.payload, frame.len);
				s_expected_seq++;
				OtaUart_SendResponse(OTA_ACK, frame.seq);
			}
			else if (s_expected_seq > 0 && frame.seq == s_expected_seq - 1)
			{
				/* 重发的上一包 (ACK 丢失), 幂等地重发 ACK */
				OtaUart_SendResponse(OTA_ACK, frame.seq);
			}
			else
			{
				OtaUart_SendResponse(OTA_NAK, (uint16_t)s_expected_seq);
			}
		}
		else if (frame.cmd == OTA_CMD_END)
		{
			if (VerifyDownloadArea())
			{
				if (BackupCurrentApp())
				{
					ota_param_t p;
					memset(&p, 0, sizeof(p));
					p.magic = OTA_PARAM_MAGIC;
					p.flag = OTA_FLAG_PENDING;
					p.new_size = s_total_size;
					p.new_crc32 = s_expected_crc32;
					p.new_version = s_new_version;
					p.backup_size = OTA_APP_MAX_SIZE;
					p.old_version = ((uint32_t)OTA_APP_VERSION_MAJOR << 16) | OTA_APP_VERSION_MINOR;

					OtaFlash_WriteParam(&p);

					OtaUart_SendResponse(OTA_RESP_VERIFY_OK, 0);
					OtaLog_Str("[APP] OTA done, reset to bootloader...");
					OtaLog_CRLF();
					Delay_ms(5);
					NVIC_SystemReset();
				}
				else
				{
					OtaUart_SendResponse(OTA_RESP_VERIFY_ERR, 0);
					s_state = OTA_APP_IDLE;
				}
			}
			else
			{
				OtaUart_SendResponse(OTA_RESP_VERIFY_ERR, 0);
				s_state = OTA_APP_IDLE;   /* 允许上位机重传 */
			}
		}
		else if (frame.cmd == OTA_CMD_ABORT)
		{
			OtaLog_Str("[APP] OTA abort");
			OtaLog_CRLF();
			s_state = OTA_APP_IDLE;
		}
		break;
	}
}
