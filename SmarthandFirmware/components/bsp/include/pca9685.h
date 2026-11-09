/*
 * Copyright (c) 2022 Nick Ward <nix.ward@gmail.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * 基于 Zephyr Project 上游 PCA9685 PWM 驱动移植至 ESP-IDF。
 * 移植修改：公开 ESP-IDF 类型，并以不透明 handle 提供多实例接口。
 */

#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9685_I2C_ADDR_DEFAULT 0x40
#define PCA9685_CHANNEL_COUNT 16

typedef struct pca9685_device *pca9685_handle_t;

typedef struct {
    i2c_master_bus_handle_t bus;
    uint8_t address;
    uint32_t scl_speed_hz;
    uint32_t oscillator_hz;
    uint32_t default_frequency_hz;
} pca9685_config_t;

typedef struct {
    esp_err_t (*init)(pca9685_handle_t handle);
    esp_err_t (*set_frequency_hz)(pca9685_handle_t handle, uint32_t frequency_hz);
    esp_err_t (*set_servo_pulse_us)(pca9685_handle_t handle, uint8_t channel,
                                    uint32_t pulse_us);
    esp_err_t (*all_off)(pca9685_handle_t handle);
} pca9685_ops_t;

extern const pca9685_ops_t pca9685_ops;

esp_err_t pca9685_create(const pca9685_config_t *config, pca9685_handle_t *out_handle);
esp_err_t pca9685_delete(pca9685_handle_t handle);

#ifdef __cplusplus
}
#endif
