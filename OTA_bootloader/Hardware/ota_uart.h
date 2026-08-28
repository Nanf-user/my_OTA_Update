#ifndef __OTA_UART_H
#define __OTA_UART_H

#include "stm32f10x.h"
#include "ota_config.h"

typedef struct {
	uint8_t  cmd;
	uint16_t seq;
	uint16_t len;
	uint8_t *payload;      /* 指向内部缓冲, 仅在下次解析前有效 */
} ota_frame_t;

void    OtaUart_Init(void);
uint8_t OtaUart_Poll(ota_frame_t *frame);
void    OtaUart_SendResponse(uint8_t code, uint16_t seq);

#endif
