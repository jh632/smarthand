/*
 * Copyright (c) 2022 Nick Ward <nix.ward@gmail.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * 基于 Zephyr Project 上游 PCA9685 PWM 驱动移植至 ESP-IDF。
 * 移植修改：使用 ESP-IDF I2C master API、FreeRTOS mutex 和不透明 handle。
 * public ops 仅开放受限的舵机 PWM 操作。
 */

#include "pca9685.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define PCA9685_REG_MODE1 0x00
#define PCA9685_REG_MODE2 0x01
#define PCA9685_REG_LED0_ON_L 0x06
#define PCA9685_REG_ALL_LED_ON_L 0xFA
#define PCA9685_REG_PRE_SCALE 0xFE

#define PCA9685_MODE1_RESTART (1U << 7)
#define PCA9685_MODE1_AI (1U << 5)
#define PCA9685_MODE1_SLEEP (1U << 4)
#define PCA9685_MODE2_OUTDRV (1U << 2)
#define PCA9685_LED_FULL_ON (1U << 4)
#define PCA9685_LED_FULL_OFF (1U << 4)

#define PCA9685_OSCILLATOR_HZ_DEFAULT 25000000U
#define PCA9685_OSCILLATOR_HZ_MAX 50000000U
#define PCA9685_FREQUENCY_HZ_DEFAULT 50U
#define PCA9685_I2C_SPEED_HZ_DEFAULT 400000U
#define PCA9685_I2C_TIMEOUT_MS 1000
#define PCA9685_PRESCALE_MIN 3U
#define PCA9685_PRESCALE_MAX 255U

struct pca9685_device {
    i2c_master_dev_handle_t i2c_device;
    SemaphoreHandle_t mutex;
    uint32_t oscillator_hz;
    uint32_t frequency_hz;
    uint32_t default_frequency_hz;
    uint16_t active_channels;
    uint8_t prescale;
    bool initialized;
    bool frequency_configured;
};

static esp_err_t write_register(pca9685_handle_t handle, uint8_t reg, uint8_t value)
{
    uint8_t data[] = { reg, value };
    return i2c_master_transmit(handle->i2c_device, data, sizeof(data),
                               PCA9685_I2C_TIMEOUT_MS);
}

static esp_err_t read_register(pca9685_handle_t handle, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(handle->i2c_device, &reg, sizeof(reg), value,
                                       sizeof(*value), PCA9685_I2C_TIMEOUT_MS);
}

static uint32_t ceil_div(uint64_t numerator, uint64_t denominator)
{
    return (uint32_t)((numerator + denominator - 1U) / denominator);
}

static esp_err_t write_all_off(pca9685_handle_t handle)
{
    uint8_t data[] = { PCA9685_REG_ALL_LED_ON_L, 0, 0,
                       0, PCA9685_LED_FULL_OFF };
    esp_err_t err = i2c_master_transmit(handle->i2c_device, data, sizeof(data),
                                        PCA9685_I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
        handle->active_channels = 0;
    }
    return err;
}

static esp_err_t set_frequency_locked(pca9685_handle_t handle,
                                     uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t divider = ceil_div(handle->oscillator_hz,
                                (uint64_t)frequency_hz * 4096U);
    if (divider < PCA9685_PRESCALE_MIN + 1U ||
        divider > PCA9685_PRESCALE_MAX + 1U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t prescale = (uint8_t)(divider - 1U);
    if (handle->frequency_configured && handle->prescale == prescale) {
        handle->frequency_hz = frequency_hz;
        return ESP_OK;
    }
    if (handle->active_channels != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t mode;
    esp_err_t err = read_register(handle, PCA9685_REG_MODE1, &mode);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t sleep_mode = (mode | PCA9685_MODE1_SLEEP | PCA9685_MODE1_AI) &
                         (uint8_t)~PCA9685_MODE1_RESTART;
    uint8_t restart_mode = mode & PCA9685_MODE1_RESTART;
    handle->frequency_configured = false;
    err = write_register(handle, PCA9685_REG_MODE1, sleep_mode);
    if (err == ESP_OK) {
        err = write_register(handle, PCA9685_REG_PRE_SCALE, prescale);
    }
    uint8_t wake_mode = (mode & (uint8_t)~(PCA9685_MODE1_SLEEP |
                                           PCA9685_MODE1_RESTART)) |
                        PCA9685_MODE1_AI;
    if (err == ESP_OK) {
        err = write_register(handle, PCA9685_REG_MODE1, wake_mode);
    }
    if (err == ESP_OK) {
        esp_rom_delay_us(500);
        err = write_register(handle, PCA9685_REG_MODE1,
                             wake_mode | restart_mode);
    }
    if (err == ESP_OK) {
        handle->prescale = prescale;
        handle->frequency_hz = frequency_hz;
        handle->frequency_configured = true;
    }
    return err;
}

static esp_err_t pca9685_init_impl(pca9685_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    uint8_t mode1;
    esp_err_t err = read_register(handle, PCA9685_REG_MODE1, &mode1);
    if (err == ESP_OK) {
        err = write_register(handle, PCA9685_REG_MODE1,
                             mode1 | PCA9685_MODE1_AI);
    }
    if (err == ESP_OK) {
        err = write_register(handle, PCA9685_REG_MODE2, PCA9685_MODE2_OUTDRV);
    }
    if (err == ESP_OK) {
        err = set_frequency_locked(handle, handle->default_frequency_hz);
    }
    if (err == ESP_OK) {
        err = write_all_off(handle);
    }
    if (err == ESP_OK) {
        handle->initialized = true;
    }

    xSemaphoreGive(handle->mutex);
    return err;
}

static esp_err_t pca9685_set_frequency_impl(pca9685_handle_t handle,
                                            uint32_t frequency_hz)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t err = handle->initialized
                        ? set_frequency_locked(handle, frequency_hz)
                        : ESP_ERR_INVALID_STATE;
    xSemaphoreGive(handle->mutex);
    return err;
}

static esp_err_t pca9685_set_servo_pulse_impl(pca9685_handle_t handle,
                                              uint8_t channel, uint32_t pulse_us)
{
    if (handle == NULL || channel >= PCA9685_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    if (!handle->initialized || !handle->frequency_configured) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    uint64_t divider = (uint64_t)handle->prescale + 1U;
    uint64_t max_pulse_us = (4096ULL * divider * 1000000ULL) /
                            handle->oscillator_hz;
    if (pulse_us > max_pulse_us) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t count = ((uint64_t)pulse_us * handle->oscillator_hz +
                      divider * 1000000ULL - 1U) /
                     (divider * 1000000ULL);
    uint8_t reg = PCA9685_REG_LED0_ON_L + channel * 4U;
    uint8_t data[] = {
        reg,
        0,
        count >= 4096U ? PCA9685_LED_FULL_ON : 0,
        (uint8_t)count,
        pulse_us == 0 ? PCA9685_LED_FULL_OFF : (uint8_t)((count >> 8) & 0x0FU),
    };
    esp_err_t err = i2c_master_transmit(handle->i2c_device, data, sizeof(data),
                                        PCA9685_I2C_TIMEOUT_MS);
    if (pulse_us == 0) {
        if (err == ESP_OK) {
            handle->active_channels &= (uint16_t)~(1U << channel);
        }
    } else {
        handle->active_channels |= (uint16_t)(1U << channel);
    }
    xSemaphoreGive(handle->mutex);
    return err;
}

static esp_err_t pca9685_all_off_impl(pca9685_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t err = handle->initialized ? write_all_off(handle)
                                        : ESP_ERR_INVALID_STATE;
    xSemaphoreGive(handle->mutex);
    return err;
}

const pca9685_ops_t pca9685_ops = {
    .init = pca9685_init_impl,
    .set_frequency_hz = pca9685_set_frequency_impl,
    .set_servo_pulse_us = pca9685_set_servo_pulse_impl,
    .all_off = pca9685_all_off_impl,
};

esp_err_t pca9685_create(const pca9685_config_t *config,
                         pca9685_handle_t *out_handle)
{
    if (config == NULL || out_handle == NULL || config->bus == NULL ||
        (config->address != 0 &&
         (config->address < PCA9685_I2C_ADDR_DEFAULT || config->address > 0x7FU)) ||
        config->oscillator_hz > PCA9685_OSCILLATOR_HZ_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    pca9685_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->oscillator_hz = config->oscillator_hz != 0
                                ? config->oscillator_hz
                                : PCA9685_OSCILLATOR_HZ_DEFAULT;
    handle->default_frequency_hz = config->default_frequency_hz != 0
                                       ? config->default_frequency_hz
                                       : PCA9685_FREQUENCY_HZ_DEFAULT;
    handle->mutex = xSemaphoreCreateMutex();
    if (handle->mutex == NULL) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->address != 0
                              ? config->address
                              : PCA9685_I2C_ADDR_DEFAULT,
        .scl_speed_hz = config->scl_speed_hz != 0
                            ? config->scl_speed_hz
                            : PCA9685_I2C_SPEED_HZ_DEFAULT,
    };
    esp_err_t err = i2c_master_bus_add_device(config->bus, &device_config,
                                              &handle->i2c_device);
    if (err != ESP_OK) {
        vSemaphoreDelete(handle->mutex);
        free(handle);
        return err;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t pca9685_delete(pca9685_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t err = i2c_master_bus_rm_device(handle->i2c_device);
    xSemaphoreGive(handle->mutex);
    if (err != ESP_OK) {
        return err;
    }

    vSemaphoreDelete(handle->mutex);
    free(handle);
    return ESP_OK;
}
