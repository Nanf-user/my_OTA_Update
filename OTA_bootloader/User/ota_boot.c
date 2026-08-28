#include "ota_boot.h"
#include "ota_config.h"
#include "ota_flash.h"
#include "ota_log.h"
#include "crc32.h"
#include "W25Q64.h"
#include <string.h>

static uint8_t s_buf[OTA_PKT_DATA_SIZE];

/* 校验 W25Q64 下载区 CRC32 */
static uint8_t VerifyDownload(uint32_t size, uint32_t expect)
{
	uint32_t crc = 0xFFFFFFFFUL, off, n;

	for (off = 0; off < size; off += OTA_PKT_DATA_SIZE)
	{
		n = (size - off) > OTA_PKT_DATA_SIZE ? OTA_PKT_DATA_SIZE : (size - off);
		W25Q64_ReadData(OTA_EXT_DL_ADDR + off, s_buf, n);
		crc = CRC32_Update(crc, s_buf, n);
	}
	crc ^= 0xFFFFFFFFUL;
	return (crc == expect) ? 1 : 0;
}

/* 校验固件头 (堆栈指针 + 复位向量), 防止把 .hex 等错误文件当固件烧入 */
static uint8_t VerifyFirmwareHeader(void)
{
	uint8_t hdr[8];
	uint32_t sp, pc;

	W25Q64_ReadData(OTA_EXT_DL_ADDR, hdr, 8);
	sp = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
	pc = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);

	if (sp < 0x20000000UL || sp > 0x20005000UL)
	{
		return 0;   /* 堆栈指针非法 */
	}
	if (pc < OTA_FLASH_BASE || pc > OTA_FLASH_BASE + 0x00010000UL)
	{
		return 0;   /* 复位向量非法 */
	}
	return 1;
}

/* 从 W25Q64 下载区写入内部 Flash APP 区 */
static void WriteAppFromDownload(uint32_t size)
{
	uint32_t off, n;

	for (off = 0; off < size; off += OTA_PKT_DATA_SIZE)
	{
		n = (size - off) > OTA_PKT_DATA_SIZE ? OTA_PKT_DATA_SIZE : (size - off);
		W25Q64_ReadData(OTA_EXT_DL_ADDR + off, s_buf, n);
		OtaFlash_WriteRange(OTA_APP_ADDR + off, s_buf, n);
	}
}

/* 从 W25Q64 备份区恢复 APP 区 */
static void RestoreAppFromBackup(uint32_t size)
{
	uint32_t off, n;

	for (off = 0; off < size; off += OTA_PKT_DATA_SIZE)
	{
		n = (size - off) > OTA_PKT_DATA_SIZE ? OTA_PKT_DATA_SIZE : (size - off);
		W25Q64_ReadData(OTA_EXT_BACKUP_ADDR + off, s_buf, n);
		OtaFlash_WriteRange(OTA_APP_ADDR + off, s_buf, n);
	}
}

/* 校验内部 Flash APP 区 CRC32 */
static uint8_t VerifyAppRegion(uint32_t size, uint32_t expect)
{
	uint32_t crc = 0xFFFFFFFFUL, off, n;

	for (off = 0; off < size; off += OTA_PKT_DATA_SIZE)
	{
		n = (size - off) > OTA_PKT_DATA_SIZE ? OTA_PKT_DATA_SIZE : (size - off);
		memcpy(s_buf, (const void *)(OTA_APP_ADDR + off), n);
		crc = CRC32_Update(crc, s_buf, n);
	}
	crc ^= 0xFFFFFFFFUL;
	return (crc == expect) ? 1 : 0;
}

/**
  * 函    数：Bootloader 主流程 (不返回)
  * 流    程：读参数区 -> 有升级任务则校验/烧写/回退 -> 跳转 APP
  */
void OtaBoot_Run(void)
{
	ota_param_t p;

	OtaFlash_ReadParam(&p);

	if (p.magic != OTA_PARAM_MAGIC || p.flag != OTA_FLAG_PENDING)
	{
		/* 无升级任务, 直接跳转 APP */
		uint32_t sp = *(volatile uint32_t *)OTA_APP_ADDR;
		if (sp == 0xFFFFFFFFUL || sp < 0x20000000UL || sp > 0x20005000UL)
		{
			OtaLog_Str("[BL] No valid app!");
			OtaLog_CRLF();
			while (1)
			{
			}
		}
		OtaLog_Str("[BL] No OTA pending, boot normally.");
		OtaLog_CRLF();
		OtaJumpToApp(OTA_APP_ADDR);
		return;
	}

	OtaLog_Str("[BL] OTA pending, size="); OtaLog_Num(p.new_size);
	OtaLog_Str(" ver="); OtaLog_Num(p.new_version >> 16); OtaLog_Str("."); OtaLog_Num(p.new_version & 0xFFFF);
	OtaLog_CRLF();

	/* 1. 校验下载区 (未擦 APP 前先校验, 失败则 APP 仍完好) */
	OtaLog_Str("[BL] Verify download area...");
	OtaLog_CRLF();
	if (!VerifyDownload(p.new_size, p.new_crc32))
	{
		OtaLog_Str("[BL] Download CRC mismatch, boot old app.");
		OtaLog_CRLF();
		OtaFlash_ClearFlag();
		OtaJumpToApp(OTA_APP_ADDR);
		return;
	}

	/* 1.5 校验固件头 (SP + 复位向量), 防止把 .hex 等错误文件当固件烧入 */
	OtaLog_Str("[BL] Verify firmware header...");
	OtaLog_CRLF();
	if (!VerifyFirmwareHeader())
	{
		OtaLog_Str("[BL] Invalid firmware header, abort.");
		OtaLog_CRLF();
		OtaFlash_ClearFlag();
		OtaJumpToApp(OTA_APP_ADDR);
		return;
	}

	/* 2. 擦除 APP 区并写入新固件 */
	OtaLog_Str("[BL] Erase APP & flash new firmware...");
	OtaLog_CRLF();
	OtaFlash_EraseRange(OTA_APP_ADDR, p.new_size);
	WriteAppFromDownload(p.new_size);

	/* 3. 校验 APP 区 */
	OtaLog_Str("[BL] Verify APP region...");
	OtaLog_CRLF();
	if (VerifyAppRegion(p.new_size, p.new_crc32))
	{
		OtaLog_Str("[BL] Upgrade success, jump to new APP.");
		OtaLog_CRLF();
		OtaFlash_ClearFlag();
		OtaJumpToApp(OTA_APP_ADDR);
		return;
	}

	/* 失败: 从备份区回退 */
	OtaLog_Str("[BL] APP verify fail, restore backup...");
	OtaLog_CRLF();
	if (p.backup_size > 0)
	{
		OtaFlash_EraseRange(OTA_APP_ADDR, p.backup_size);
		RestoreAppFromBackup(p.backup_size);
		OtaLog_Str("[BL] Restore done.");
		OtaLog_CRLF();
	}
	OtaFlash_ClearFlag();
	OtaJumpToApp(OTA_APP_ADDR);
}
