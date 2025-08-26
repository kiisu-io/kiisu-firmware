// Kiisu Manager - Settings protocol client
#include "settings_client.h"
#include <furi_hal_power.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <bit_lib/bit_lib.h>

#define RETRIES 2
#define READY_TIMEOUT_MS 50
#define OP_TIMEOUT_MS 150

static bool i2c_write_mem_retry(const FuriHalI2cBusHandle* bus, uint8_t addr, uint8_t reg,
                                const uint8_t* data, size_t len, uint32_t timeout) {
    for(int i = 0; i <= RETRIES; i++) {
        if(furi_hal_i2c_write_mem(bus, addr, reg, data, len, timeout)) return true;
        uint8_t buf[1 + 32];
        if(len <= sizeof(buf) - 1) {
            buf[0] = reg;
            memcpy(buf + 1, data, len);
            if(furi_hal_i2c_tx(bus, addr, buf, len + 1, timeout)) return true;
        }
        furi_delay_ms(2);
    }
    return false;
}

static bool i2c_read_mem_restart(const FuriHalI2cBusHandle* bus, uint8_t addr, uint8_t reg,
                                 uint8_t* data, size_t len, uint32_t timeout) {
    if(!furi_hal_i2c_tx_ext(
           bus, addr, false, &reg, 1, FuriHalI2cBeginStart, FuriHalI2cEndAwaitRestart, timeout))
        return false;
    return furi_hal_i2c_rx_ext(
        bus, addr, false, data, len, FuriHalI2cBeginRestart, FuriHalI2cEndStop, timeout);
}

static bool i2c_read_mem_retry(const FuriHalI2cBusHandle* bus, uint8_t addr, uint8_t reg,
                               uint8_t* data, size_t len, uint32_t timeout) {
    for(int i = 0; i <= RETRIES; i++) {
        if(i2c_read_mem_restart(bus, addr, reg, data, len, timeout)) return true;
        if(furi_hal_i2c_trx(bus, addr, &reg, 1, data, len, timeout)) return true;
        furi_delay_ms(2);
    }
    return false;
}

static bool ensure_otg_power(void) {
    if(furi_hal_power_is_otg_enabled()) return true;
    return furi_hal_power_enable_otg();
}

static void force_i2c_weak_pullups(bool enable) {
    if(enable) {
        furi_hal_gpio_init_ex(
            &gpio_i2c_power_sda, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C1);
        furi_hal_gpio_init_ex(
            &gpio_i2c_power_scl, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C1);
    } else {
        furi_hal_gpio_init_ex(
            &gpio_i2c_power_sda, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C1);
        furi_hal_gpio_init_ex(
            &gpio_i2c_power_scl, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C1);
    }
}

bool kiisu_settings_probe_ready(void) {
    if(!ensure_otg_power()) return false;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    force_i2c_weak_pullups(true);
    bool ok = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, KIISU_SETTINGS_ADDR, READY_TIMEOUT_MS);
    force_i2c_weak_pullups(false);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    return ok;
}

bool kiisu_settings_write_u8(uint8_t reg, uint8_t value) {
    if(!ensure_otg_power()) return false;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    force_i2c_weak_pullups(true);
    bool ok = i2c_write_mem_retry(&furi_hal_i2c_handle_power, KIISU_SETTINGS_ADDR, reg, &value, 1, OP_TIMEOUT_MS);
    force_i2c_weak_pullups(false);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    return ok;
}

bool kiisu_settings_read_u8(uint8_t reg, uint8_t* value) {
    if(!value) return false;
    if(!ensure_otg_power()) return false;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    force_i2c_weak_pullups(true);
    bool ok = i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_SETTINGS_ADDR, reg, value, 1, OP_TIMEOUT_MS);
    force_i2c_weak_pullups(false);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    return ok;
}
