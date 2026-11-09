# SmarthandFirmware

基于 ESP-IDF 的 ESP32-C3 固件项目，使用 C 语言。设备启动时输出基本启动日志。

## 开发环境

- ESP-IDF v6.1
- ESP32-C3 开发板

运行命令前，需要在当前终端激活 ESP-IDF 开发环境。

## 编译

在项目目录执行：

```bash
idf.py build
```

`sdkconfig.defaults` 将默认主控配置为 `esp32c3`。构建结果保存在 `build/`，应用固件为 `build/SmarthandFirmware.bin`。

## 烧录与串口监视

将下面的串口路径替换为开发板对应的设备路径：

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

使用 `Ctrl+]` 退出串口监视。

## 配置

```bash
idf.py menuconfig
```

本地配置保存在 `sdkconfig`。需要共享的配置写入 `sdkconfig.defaults`；可以使用以下命令保存当前配置中与默认值不同的选项：

```bash
idf.py save-defconfig
```

## 目录

```text
SmarthandFirmware/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   └── main.c
├── sdkconfig.defaults
├── .gitignore
└── README.md
```
