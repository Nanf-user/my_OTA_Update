# STM32F103 OTA 升级项目

基于 `STM32F103C8`（64KB Flash / 20KB RAM）+ `W25Q64`（8MB SPI Flash）的 OTA 升级方案。

- `OTA_bootloader` —— 启动引导 + 升级/回退
- `OTA_app` —— 业务应用 + 接收固件
- `ota_server.py` —— 上位机（模拟 ESP32），通过 UART 发送固件

---

## 1. Flash 布局

### 内部 Flash（64KB）

| 区域 | 地址范围 | 大小 | 说明 |
|------|----------|------|------|
| Bootloader | `0x08000000` ~ `0x080043FF` | 17KB | 引导 + 升级 |
| APP | `0x08004400` ~ `0x0800FBFF` | 约 46KB | 业务应用 |
| 参数区 | `0x0800FC00` ~ `0x0800FFFF` | 1KB | OTA 标志 / 固件信息 |

> 所有地址均在 `User/ota_config.h` 中定义，可整体调整。

### W25Q64 外部 Flash（8MB）

| 区域 | 地址范围 | 大小 | 说明 |
|------|----------|------|------|
| 下载区 | `0x000000` ~ `0x07FFFF` | 512KB | 暂存新固件 |
| 备份区 | `0x080000` ~ `0x0FFFFF` | 512KB | 备份旧固件（回退用） |

---

## 2. OTA 流程（对应 process_1.txt）

```
云端/APP 触发 OTA
   → ESP32 HTTP GET 下载固件
   → ESP32 分包(每包256B) 通过 UART 发给 STM32
   → APP 逐包写入 W25Q64 下载区
   → 全部收完, APP 校验 CRC32
       校验失败 → 通知上位机重传
       校验成功 → 备份当前 APP 到备份区
   → 写 OTA 标志到内部 Flash 参数区
   → 软复位进入 Bootloader
   → Bootloader 从下载区读新固件 → 擦除 APP 区 → 逐页写入 → CRC 校验
       校验失败 → 从备份区恢复旧固件
       校验成功 → 清除标志 → 跳转 APP 运行
```

---

## 3. UART 传输协议

波特率 `115200`，每包数据 `256` 字节。

### 帧格式（上位机 → STM32）

| 字段 | 长度 | 说明 |
|------|------|------|
| 头 | 2 | `0xAA 0x55` |
| cmd | 1 | `0x01 START` / `0x02 DATA` / `0x03 END` / `0x04 ABORT` |
| seq | 2 | 大端；START=总包数，DATA=包序号 |
| len | 2 | 大端；payload 长度（≤256） |
| payload | len | 数据 |
| crc16 | 2 | 大端；对 cmd..payload 的 CRC16(Modbus) |

- **START** payload（12 字节）：`size(4) crc32(4) ver_major(2) ver_minor(2)`，均为大端
- **DATA** payload：固件分片
- **END** payload：空

### 应答格式（STM32 → 上位机）

`[0xAA][0x55][code][seq_H][seq_L]`

| code | 含义 |
|------|------|
| `0x06` | ACK（DATA 接收成功） |
| `0x15` | NAK（序号不符，需重传） |
| `0x02` | START 成功 |
| `0x03` | START 失败（尺寸非法） |
| `0x04` | 校验通过，即将软复位升级 |
| `0x05` | 校验失败，可重传 |

---

## 4. 编译与烧写（对应 process_2.txt）

### 4.1 确认链接地址

工程已按下面配置（若手动新建工程请核对）：

- **Bootloader**：`IROM1` 起始 `0x08000000` 大小 `0x4400`
- **APP**：`IROM1` 起始 `0x08004400` 大小 `0xB800`

> Keil 中查看：`Options for Target → Target → Read/Only Memory Areas`

### 4.2 编译顺序

1. 打开 `OTA_bootloader/Project.uvprojx`，`Rebuild` 得到 `Objects/Project.hex`
2. 打开 `OTA_app/Project.uvprojx`，`Rebuild` 得到 `Objects/Project.hex`

> 若 Keil 里看不到新增的 `ota_*.c / crc32.c` 文件，请在对应 Group 上
> 右键 `Add Existing Files`，添加：
> - `Hardware/` 下：`crc32.c`、`ota_log.c`、`ota_uart.c`
> - `User/` 下：`ota_flash.c`、`ota_app.c`（仅 APP）/ `ota_boot.c`（仅 Bootloader）

### 4.3 烧写

1. ST-Link 烧写 `OTA_bootloader/Objects/Project.hex` 到 `0x08000000`
2. ST-Link 烧写 `OTA_app/Objects/Project.hex` 到 `0x08004400`
   （用 Keil 的 `Download` 会按工程配置地址自动烧写，不会破坏 Bootloader）

### 4.4 串口日志

复位后应看到：

```
[BL] Bootloader v1.0 started
[BL] W25Q64 ID: 0xEF4017
[BL] No OTA pending, boot normally.
[APP] Application v1.0 started
[APP] W25Q64 ID: 0xEF4017
```

---

## 5. OTA 升级测试

1. 修改 `User/ota_config.h` 中的 `OTA_APP_VERSION_MAJOR/MINOR`，重新编译 `OTA_app`
2. 连接 STM32 串口（USART1, PA9/PA10）到 PC
3. 运行上位机：
   ```bash
   pip install pyserial
   python ota_server.py COM3 OTA_app/Objects/Project.bin 115200
   ```
   > 需要 `Project.bin`。可在 Keil `User` 选项卡勾选 `Create HEX` 后用工具转 bin，
   > 或改用 Keil 生成 bin：`Options → User → After Build/Rebuild` 添加
   > `fromelf --bin -o "$L@L.bin" "#L"`。

4. 观察串口日志：APP 接收 → 备份 → 写标志 → 软复位 → Bootloader 升级 → 跳转新版 APP

---

## 6. 常见问题

- **烧写后一直停在 `[BL] No valid app!`**：说明 APP 未正确烧到 `0x08004400`，先烧 Bootloader 再烧 APP。
- **OTA 后 APP 无法运行**：核对 APP 工程 `IROM1` 起始地址是否为 `0x08004400`；APP 的 `main()` 首行必须保留 `SCB->VTOR = OTA_APP_ADDR;`。
- **升级失败自动回退**：Bootloader 会从 W25Q64 备份区恢复旧固件，需保证下载区 CRC 与烧写过程正常。
- **Bootloader 超 17KB 溢出**：在 Keil 将优化等级改为 `-O2/-O3`，或增大 `OTA_BOOT_SIZE` 并同步调整 `OTA_APP_ADDR`。
