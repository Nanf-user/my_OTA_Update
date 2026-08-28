#ifndef __W25Q64_H
#define __W25Q64_H
#include "generic.h"
void W25Q64_Init(void);
void W25Q64_ReadID(uint8_t *MID, uint16_t *DID);
void W25Q64_PageProgram(uint32_t Address, uint8_t *DataArray, uint16_t Count);
void W25Q64_SectorErase(uint32_t Address);
void W25Q64_ReadData(uint32_t Address, uint8_t *DataArray, uint32_t Count);
void W25Q64_ChipErase(void);
void W25Q64_WriteBuffer(uint32_t addr, uint8_t *buf, uint32_t size);
void W25Q64_ReadBuffer(uint32_t Address, uint8_t *Buffer, uint32_t Length);

#endif
