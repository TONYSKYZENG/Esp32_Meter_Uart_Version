#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ESP32-S2 电表上位机测试脚本。

功能：让电表测量指定时长（默认 10 秒），然后读取这段时间的耗电（累计能量）。

协议来源：main/uart0/uart0.c
    0x01 START  开始测量（累计能量 g_power 与峰值 g_peak 清零）
    0x02 STOP   停止测量
    0x06 E      回传累计能量 g_power（原始 8 字节 double）
    0x07 PEAK   回传峰值 g_peak（原始 8 字节 double）

注意：固件直接把 double 的原始内存字节通过串口写出，ESP32-S2 为小端，
      因此这里用 struct.unpack('<d', ...) 解析。

用法：
    python3 test.py                      # 自动查找串口，测量 10 秒
    python3 test.py /dev/ttyUSB0         # 指定串口
    python3 test.py /dev/ttyUSB0 -t 5    # 指定串口并测量 5 秒
    python3 test.py --list               # 列出可用串口
"""

import argparse
import struct
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("缺少依赖 pyserial，请先安装：pip install pyserial")

# ---- 协议命令 ----
CMD_START = 0x01
CMD_STOP = 0x02
CMD_ENERGY = 0x06  # 累计能量
CMD_PEAK = 0x07    # 峰值

DEFAULT_BAUD = 1152000
DEFAULT_SECONDS = 10.0

DOUBLE_SIZE = 8
READ_TIMEOUT_S = 1.0
# 常见的 USB 转串口设备名关键字，用于自动挑选串口
PORT_HINTS = ("usbserial", "usbmodem", "wchusb", "cp210", "ch340", "ttyusb", "ttyacm")


def list_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("未发现任何串口")
        return
    for p in ports:
        print(f"{p.device}\t{p.description}")


def find_port():
    """自动挑选一个看起来像 USB 串口的设备。"""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        sys.exit("未找到串口，请用参数指定，例如：python3 test.py /dev/ttyUSB0")
    for p in ports:
        if any(k in p.device.lower() for k in PORT_HINTS):
            return p.device
    return ports[0].device


def read_double(ser):
    """从串口读取 8 字节并解析为小端 double。"""
    buf = b""
    deadline = time.time() + READ_TIMEOUT_S
    while len(buf) < DOUBLE_SIZE and time.time() < deadline:
        chunk = ser.read(DOUBLE_SIZE - len(buf))
        if chunk:
            buf += chunk
    if len(buf) < DOUBLE_SIZE:
        raise TimeoutError(f"只收到 {len(buf)} 字节，期望 {DOUBLE_SIZE} 字节")
    return struct.unpack("<d", buf)[0]


def send_cmd(ser, cmd):
    ser.write(bytes([cmd]))
    ser.flush()


def measure(port, seconds, baud):
    print(f"串口 {port} @ {baud} 波特率，测量 {seconds:g} 秒 ...")
    with serial.Serial(port, baud, timeout=0.1) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()

        # 1. 开始测量
        send_cmd(ser, CMD_START)
        print("已发送 START，测量中 ...")

        # 2. 等待指定时长，期间固件在后台累计能量
        time.sleep(seconds)

        # 3. 停止测量
        send_cmd(ser, CMD_STOP)
        time.sleep(0.1)

        # 4. 读取累计能量与峰值
        send_cmd(ser, CMD_ENERGY)
        energy = read_double(ser)

        send_cmd(ser, CMD_PEAK)
        peak = read_double(ser)

    print("-" * 40)
    print(f"测量时长   : {seconds:g} s")
    print(f"累计耗电   : {energy:.3f} J")
    print(f"平均功率   : {energy / seconds:.3f} W")
    # 峰值来自固件 g_w1，按其显示单位输出为 mW
    print(f"峰值功率   : {peak:.3f} mW")


def main():
    parser = argparse.ArgumentParser(description="ESP32-S2 电表上位机测试脚本")
    parser.add_argument("port", nargs="?", help="串口设备，例如 /dev/ttyUSB0；省略则自动查找")
    parser.add_argument("-t", "--time", type=float, default=DEFAULT_SECONDS,
                        help=f"测量时长（秒），默认 {DEFAULT_SECONDS:g}")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD,
                        help=f"波特率，默认 {DEFAULT_BAUD}")
    parser.add_argument("--list", action="store_true", help="列出可用串口后退出")
    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    port = args.port or find_port()
    try:
        measure(port, args.time, args.baud)
    except KeyboardInterrupt:
        print("\n已中断")
    except (serial.SerialException, TimeoutError) as exc:
        sys.exit(f"通信失败：{exc}")


if __name__ == "__main__":
    main()
