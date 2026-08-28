#include "ota_uart.h"

/* ================= OTA 下载串口 = USART2 (PA2=TX, PA3=RX), 与日志 USART1 分离 ================= */

/* USART1 只做日志输出(TX)。底层 Serial.c 仍开着 USART1 接收中断,
 * 这里给一个空处理, 避免 PA10 收到杂散字节时进入默认中断死循环。 */
void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
	{
		(void)USART_ReceiveData(USART1);      /* 丢弃接收到的字节 */
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

/* ---- USART2 接收环形缓冲区 (大小须为 2 的幂) ---- */
#define OTA_RX_BUF_SIZE   1024

static volatile uint8_t  s_rx_buf[OTA_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

/* ---- 帧解析状态机 ---- */
typedef enum {
	ST_HEAD0, ST_HEAD1, ST_CMD, ST_SEQ_H, ST_SEQ_L,
	ST_LEN_H, ST_LEN_L, ST_DATA, ST_CRC_H, ST_CRC_L
} ota_rx_state_t;

static ota_rx_state_t s_state = ST_HEAD0;
static uint8_t  s_cmd = 0;
static uint16_t s_seq = 0;
static uint16_t s_len = 0;
static uint16_t s_idx = 0;
static uint16_t s_crc = 0;       /* 运行中的 CRC16 */
static uint16_t s_crc_rx = 0;    /* 收到的 CRC16 */
static uint8_t  s_payload[OTA_PKT_DATA_SIZE];

/* CRC16 (Modbus) 单字节更新 */
static void crc16_update(uint8_t b)
{
	uint8_t i;
	s_crc ^= b;
	for (i = 0; i < 8; i++)
	{
		if (s_crc & 1)
		{
			s_crc = (uint16_t)((s_crc >> 1) ^ 0xA001);
		}
		else
		{
			s_crc = (uint16_t)(s_crc >> 1);
		}
	}
}

/* USART2 发送一个字节 (阻塞) */
static void ota_uart_send_byte(uint8_t b)
{
	USART_SendData(USART2, b);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}

/* USART2 接收中断: 字节入环形缓冲区 */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		uint8_t d = (uint8_t)USART_ReceiveData(USART2);
		uint16_t next = (uint16_t)((s_rx_head + 1) & (OTA_RX_BUF_SIZE - 1));
		if (next != s_rx_tail)      /* 缓冲区未满 */
		{
			s_rx_buf[s_rx_head] = d;
			s_rx_head = next;
		}
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}

void OtaUart_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/* 时钟: GPIOA + USART2 (注意 USART2 挂在 APB1 上, 不是 APB2) */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	/* PA2 = TX (复用推挽), PA3 = RX (上拉输入) */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* USART2 配置 */
	USART_InitStructure.USART_BaudRate = OTA_UART_BAUD;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART2, &USART_InitStructure);

	/* 接收中断 */
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART2, ENABLE);

	/* 复位环形缓冲区与解析状态 */
	s_rx_head = 0;
	s_rx_tail = 0;
	s_state = ST_HEAD0;
}

static uint8_t ota_uart_read(uint8_t *b)
{
	if (s_rx_head == s_rx_tail)
	{
		return 0;
	}
	*b = s_rx_buf[s_rx_tail];
	s_rx_tail = (uint16_t)((s_rx_tail + 1) & (OTA_RX_BUF_SIZE - 1));
	return 1;
}

/**
  * 函    数：从缓冲区解析出一帧完整且 CRC 正确的数据
  * 返 回 值：1 = 已解析到一帧; 0 = 暂无完整帧
  */
uint8_t OtaUart_Poll(ota_frame_t *frame)
{
	uint8_t b;

	while (ota_uart_read(&b))
	{
		switch (s_state)
		{
		case ST_HEAD0:
			if (b == OTA_FRAME_HEAD0) s_state = ST_HEAD1;
			break;
		case ST_HEAD1:
			if (b == OTA_FRAME_HEAD1) { s_state = ST_CMD; s_crc = 0xFFFF; }
			else s_state = ST_HEAD0;
			break;
		case ST_CMD:
			s_cmd = b; crc16_update(b); s_state = ST_SEQ_H; break;
		case ST_SEQ_H:
			s_seq = (uint16_t)((uint16_t)b << 8); crc16_update(b); s_state = ST_SEQ_L; break;
		case ST_SEQ_L:
			s_seq = (uint16_t)(s_seq | b); crc16_update(b); s_state = ST_LEN_H; break;
		case ST_LEN_H:
			s_len = (uint16_t)((uint16_t)b << 8); crc16_update(b); s_state = ST_LEN_L; break;
		case ST_LEN_L:
			s_len = (uint16_t)(s_len | b); crc16_update(b); s_idx = 0;
			if (s_len > OTA_PKT_DATA_SIZE) s_state = ST_HEAD0;      /* 非法长度, 重新同步 */
			else if (s_len == 0) s_state = ST_CRC_H;
			else s_state = ST_DATA;
			break;
		case ST_DATA:
			s_payload[s_idx++] = b; crc16_update(b);
			if (s_idx >= s_len) s_state = ST_CRC_H;
			break;
		case ST_CRC_H:
			s_crc_rx = (uint16_t)((uint16_t)b << 8); s_state = ST_CRC_L; break;
		case ST_CRC_L:
			s_crc_rx = (uint16_t)(s_crc_rx | b);
			if (s_crc_rx == s_crc)
			{
				frame->cmd = s_cmd;
				frame->seq = s_seq;
				frame->len = s_len;
				frame->payload = s_payload;
				s_state = ST_HEAD0;
				return 1;
			}
			s_state = ST_HEAD0;      /* CRC 错误, 丢弃并重新同步 */
			break;
		default:
			s_state = ST_HEAD0;
			break;
		}
	}
	return 0;
}

void OtaUart_SendResponse(uint8_t code, uint16_t seq)
{
	ota_uart_send_byte(OTA_FRAME_HEAD0);
	ota_uart_send_byte(OTA_FRAME_HEAD1);
	ota_uart_send_byte(code);
	ota_uart_send_byte((uint8_t)(seq >> 8));
	ota_uart_send_byte((uint8_t)(seq & 0xFF));
}
