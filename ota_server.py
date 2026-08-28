#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_server.py  ——  模拟 ESP32 通过 UART 发送固件给 STM32 (OTA 上位机)

依赖:
    pip install pyserial

用法:
    python ota_server.py <COM口> <app.bin> [波特率]

示例:
    python ota_server.py COM3 OTA_app/Objects/Project.bin 115200

协议:
    帧: [0xAA][0x55][cmd][seq_H][seq_L][len_H][len_L][payload...][crc16_H][crc16_L]
    应答: [0xAA][0x55][code][seq_H][seq_L]
"""

import sys
import time
import binascii

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial, 请先执行: pip install pyserial")

PKT_SIZE = 256
FRAME_HEAD0 = 0xAA
FRAME_HEAD1 = 0x55

CMD_START = 0x01
CMD_DATA = 0x02
CMD_END = 0x03
CMD_ABORT = 0x04

RESP_ACK = 0x06
RESP_NAK = 0x15
RESP_START_OK = 0x02
RESP_START_ERR = 0x03
RESP_VERIFY_OK = 0x04
RESP_VERIFY_ERR = 0x05

VERSION_MAJOR = 1
VERSION_MINOR = 0


def crc16_modbus(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def build_frame(cmd, seq, payload):
    body = bytes([cmd]) + seq.to_bytes(2, "big") + len(payload).to_bytes(2, "big") + payload
    crc = crc16_modbus(body)
    return bytes([FRAME_HEAD0, FRAME_HEAD1]) + body + crc.to_bytes(2, "big")


def read_response(ser, timeout=5.0):
    """在串口流中查找 AA 55 开头的应答帧(跳过日志等无关字节), 返回 (code, seq) 或 None"""
    ser.timeout = timeout
    deadline = time.time() + timeout

    prev = None
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        if prev == FRAME_HEAD0 and b[0] == FRAME_HEAD1:
            rest = ser.read(3)      # code + seq_H + seq_L
            if len(rest) < 3:
                return None
            return rest[0], (rest[1] << 8) | rest[2]
        prev = b[0]
    return None


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    port = sys.argv[1]
    fw_path = sys.argv[2]
    baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

    with open(fw_path, "rb") as f:
        fw = f.read()

    size = len(fw)
    crc32 = binascii.crc32(fw) & 0xFFFFFFFF
    total = (size + PKT_SIZE - 1) // PKT_SIZE

    if size == 0:
        print("固件为空!")
        return 1

    print("固件: %s" % fw_path)
    print("大小: %d 字节, 包数: %d, CRC32: 0x%08X" % (size, total, crc32))

    ser = serial.Serial(port, baud, timeout=1)
    ser.setDTR(False)            # 关闭 DTR/RTS, 避免打开端口时复位 STM32
    ser.setRTS(False)
    time.sleep(2.0)              # 等板子复位 + 重新启动完成
    ser.flushInput()             # 清掉开机日志
    print("打开 %s @ %d" % (port, baud))

    # ---- START (重试 3 次, 应对开机瞬间丢帧) ----
    payload = (
        size.to_bytes(4, "big")
        + crc32.to_bytes(4, "big")
        + VERSION_MAJOR.to_bytes(2, "big")
        + VERSION_MINOR.to_bytes(2, "big")
    )
    frame = build_frame(CMD_START, total, payload)
    r = None
    for _ in range(5):
        ser.flushInput()
        ser.write(frame)
        r = read_response(ser)
        if r and r[0] == RESP_START_OK:
            break
        time.sleep(1.0)
    if not r or r[0] != RESP_START_OK:
        print("START 失败: %r" % (r,))
        return 1
    print("START ok")

    # ---- DATA ----
    for seq in range(total):
        chunk = fw[seq * PKT_SIZE:(seq + 1) * PKT_SIZE]
        frame = build_frame(CMD_DATA, seq, chunk)
        ok = False
        for _ in range(3):  # 失败重发 3 次
            ser.write(frame)
            r = read_response(ser)
            if r and r[0] == RESP_ACK and r[1] == seq:
                ok = True
                break
        if not ok:
            print("DATA seq=%d 失败: %r" % (seq, r))
            return 1
        if seq % 20 == 0 or seq == total - 1:
            print("发送进度: %d/%d" % (seq + 1, total))

    # ---- END ----
    ser.write(build_frame(CMD_END, 0, b""))
    r = read_response(ser, timeout=20)
    if r and r[0] == RESP_VERIFY_OK:
        print("VERIFY_OK -> STM32 即将软复位进入 Bootloader 升级")
    elif r and r[0] == RESP_VERIFY_ERR:
        print("VERIFY_ERR (CRC 校验失败)")
    else:
        print("END 无应答: %r" % (r,))
        return 1

    ser.close()
    print("完成. 复位后应运行新版 APP.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
