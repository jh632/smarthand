# 百元灵巧手

这是一个面向低成本复刻的百元级灵巧手项目，使用 3D 打印结构件、SG90 舵机、PCA9685 PWM 驱动板和 ESP32-C3 开发板组成。项目用于记录硬件套件、结构件和固件，方便个人学习、制作和改进。

## 复刻清单

本套件包含：

1. SG90-9g 舵机 ×5，带舵机小 6 孔配件
2. PCA9685 16 路 PWM 舵机驱动板 ×1
3. ESP32-C3 开发板（焊排针）×1
4. 裁剪好的杜邦线束（满足本套件接线）×1 套
5. 502 胶水 ×1，用于固定舵机
6. 2mm 黑色高弹驱动绳 ×1 份，已裁剪为适配长度，非整卷
7. 5V 2000mAh 锂电池与 2A 充电线 ×1 套
8. 3D 打印白色 PETG 灵巧手全套结构件 ×1 套
9. 小支强力胶水 ×1，用于粘接打印件；禁止滴入活动关节，避免关节卡死
10. 关节驱动线

## 固件

固件位于 [`SmarthandFirmware`](./SmarthandFirmware)，基于 ESP-IDF v6.1，目标芯片为 ESP32-C3，使用 C 语言开发。

当前固件启动后输出基本启动日志。项目曾使用以下接线进行 PCA9685 验证：

- ESP32-C3 `GPIO0` → PCA9685 `SDA`
- ESP32-C3 `GPIO1` → PCA9685 `SCL`
- ESP32-C3 与 PCA9685 共地
- PCA9685 默认地址为 `0x40`

编译、烧录和串口监视命令：

```bash
cd SmarthandFirmware
. /path/to/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## 目录

```text
.
├── LICENSE
├── README.md
└── SmarthandFirmware/
    ├── CMakeLists.txt
    ├── main/
    │   ├── CMakeLists.txt
    │   └── main.c
    ├── sdkconfig.defaults
    └── README.md
```

## 使用许可

当前固件源码采用仓库中的 [PolyForm Noncommercial License 1.0.0](./LICENSE)，允许个人学习、研究、测试和业余制作等非商业用途。商业用途不在许可范围内，需另行取得许可。该许可文本的适用范围为软件源码。
