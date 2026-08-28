#ifndef __OTA_APP_H
#define __OTA_APP_H

void OtaApp_Init(void);
void OtaApp_Process(void);   /* 在主循环中轮询, 处理 OTA 数据帧 */

#endif
