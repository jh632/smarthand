# PCA9685

该驱动移植自 [Zephyr v4.5.0 上游 `pwm_pca9685.c`](https://github.com/zephyrproject-rtos/zephyr/blob/v4.5.0/drivers/pwm/pwm_pca9685.c)，适配 ESP-IDF 6.x I2C master API。寄存器行为以 NXP PCA9685 数据手册为准。上游贡献者和许可见本目录的 `LICENSE` 与源文件头部；本组件按 Apache-2.0 发布。

接口通过不透明的 `pca9685_handle_t` 管理实例，`pca9685_ops` 只暴露初始化、设置芯片级 PWM 频率、设置舵机脉宽和全部通道关闭。频率是芯片级共享设置；修改频率前需先调用 `all_off`。每个 I2C 地址应只创建一个 handle。

```c
pca9685_config_t config = {
    .bus = i2c_bus,
    .address = PCA9685_I2C_ADDR_DEFAULT,
    .scl_speed_hz = 400000,
    .oscillator_hz = 25000000,
    .default_frequency_hz = 50,
};
pca9685_handle_t pca;
ESP_ERROR_CHECK(pca9685_create(&config, &pca));
ESP_ERROR_CHECK(pca9685_ops.init(pca));
ESP_ERROR_CHECK(pca9685_ops.set_servo_pulse_us(pca, 0, 1500));
```

地址、I2C 速度、振荡器频率和默认频率填 `0` 时分别使用 `0x40`、400 kHz、25 MHz 和 50 Hz。`pca9685_delete()` 释放该实例注册的 I2C device 与 handle，不会删除调用方创建的 I2C bus。销毁前需确保没有其他任务正在使用该 handle。
