// Kiisu Manager - Settings protocol client (addr 0x31 over power I2C)
#pragma once

#include <furi.h>
#include <furi_hal_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

// 7-bit address per spec
#define KIISU_SETTINGS_ADDR_7B 0x31
#define KIISU_SETTINGS_ADDR    (KIISU_SETTINGS_ADDR_7B << 1)

// Registers
typedef enum {
    REG_LED_BRIGHTNESS = 0x00, // 0=Auto, else 5..100 step 5
    REG_AUTO_POWEROFF  = 0x01, // 0..7 per spec
    REG_STARTUP_COLOR  = 0x02, // 0..8 per spec
    REG_CHARGE_RAINBOW = 0x03, // 0/1
} KiisuReg;

// Values helpers
typedef enum {
    StartupColor_Purple = 0,
    StartupColor_Off,
    StartupColor_Red,
    StartupColor_Green,
    StartupColor_Blue,
    StartupColor_Yellow,
    StartupColor_Cyan,
    StartupColor_Magenta,
    StartupColor_White,
} KiisuStartupColor;

bool kiisu_settings_probe_ready(void);

// Basic read/write of single-byte registers
bool kiisu_settings_write_u8(uint8_t reg, uint8_t value);
bool kiisu_settings_read_u8(uint8_t reg, uint8_t* value);

#ifdef __cplusplus
}
#endif
