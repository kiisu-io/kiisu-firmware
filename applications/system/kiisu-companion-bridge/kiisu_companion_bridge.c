// Kiisu Companion Bridge: select a .bin and flash via power I2C updater @ 0x30 (7-bit)

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <furi_hal_i2c.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view_port.h>
#include <gui/modules/loading.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <bit_lib/bit_lib.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
// UI helpers for OS buttons
#include <gui/elements.h>
#include <string.h>

// Home image (128x64 XBM)
static const uint8_t image_ST_michelangelo_0_bits[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x58,0x01,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xaf,0x0a,0xdc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0xf0,0x17,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x3c,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0xe0,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x58,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0x2a,0x00,0x00,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x55,0x01,0xe0,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xbf,0x32,0x10,0x00,0x80,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xff,0x07,0x0c,0x00,0x00,0x00,0x01,0x00,0xfc,0xff,0x7f,0x00,0x00,0x00,0x00,0xf0,0xff,0x03,0x03,0x00,0x00,0x00,0x02,0x00,0x04,0x00,0x40,0x00,0x00,0x00,0x00,0x5c,0xff,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x04,0x00,0x58,0x00,0x00,0x00,0x00,0xab,0x5e,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x04,0x00,0x58,0x00,0x00,0x00,0xc0,0x54,0x28,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x04,0x00,0x40,0x00,0x00,0x00,0x3e,0x80,0x02,0x00,0x00,0x00,0x00,0x20,0x80,0x00,0x04,0x00,0x40,0x00,0x00,0xf8,0x01,0x40,0x10,0x00,0xe0,0x07,0x40,0xf1,0x01,0x01,0x84,0xff,0x5f,0x00,0xf0,0x07,0x00,0x80,0x00,0x00,0x54,0x1f,0x80,0xfc,0x07,0x02,0x84,0x01,0x40,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x8a,0x3b,0x00,0xff,0x1f,0x04,0xc4,0xfe,0x4e,0x01,0x00,0x00,0x00,0x20,0x02,0x00,0x44,0x7d,0x00,0xff,0x3f,0x08,0xc4,0x7e,0x4f,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0xa2,0x7e,0x00,0xf2,0xff,0x10,0xc4,0x7d,0x4f,0x02,0x00,0x00,0x00,0xa0,0x08,0x00,0x50,0x87,0x00,0xfc,0x3f,0x61,0xc4,0x73,0x4f,0x04,0x18,0x00,0x00,0x40,0x00,0x00,0xaa,0x01,0x01,0xf0,0x3f,0x86,0xe5,0xb7,0x47,0x18,0x0e,0x00,0x00,0xa0,0x02,0x00,0xd5,0x00,0x02,0xc0,0x7e,0x08,0x05,0xb0,0x47,0x20,0x04,0x00,0x00,0x40,0x01,0x00,0x6a,0x00,0x7c,0x80,0xfc,0xf0,0xf4,0xff,0x43,0x40,0x02,0x0c,0x00,0xa0,0x0a,0x00,0x35,0x00,0x80,0x01,0xf9,0x00,0x04,0x00,0x40,0x40,0x82,0x5f,0x55,0x55,0x01,0x00,0x1a,0x00,0x00,0x06,0xfa,0x00,0x04,0x00,0x40,0x80,0xfc,0xbe,0xaa,0xaa,0x0a,0x00,0x0d,0x00,0x00,0x18,0xf2,0x01,0x04,0x00,0x40,0x80,0x50,0xff,0x5f,0xf5,0x05,0x00,0x06,0x00,0x00,0x78,0xf2,0x01,0x04,0x00,0x40,0x00,0xe1,0x01,0xfe,0xff,0x2a,0x00,0x03,0x00,0x00,0xb8,0x61,0x00,0x04,0x00,0x40,0x00,0x3e,0x00,0x00,0xc0,0x05,0x00,0x01,0x00,0x00,0x38,0x00,0x00,0xfc,0xff,0x7f,0x00,0x00,0x00,0x00,0x80,0x2b,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x16,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x54,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x16
};

#define TAG "KiisuBridge"

// I2C addressing: HAL expects 8-bit address (7-bit << 1)
#define KIISU_UPD_ADDR_7B 0x30
#define KIISU_UPD_ADDR    (KIISU_UPD_ADDR_7B << 1)

// Registers per spec
enum {
	REG_PROTO_VER = 0xF0,
	REG_CMD = 0xF1,
	REG_STATUS = 0xF2,
	REG_ERROR = 0xF3,
	REG_ROLL_CRC32 = 0xF4, // ..F7
	REG_TOTAL_LEN = 0xF8,  // ..FB
	REG_RECV_LEN = 0xFC,   // ..FF
	REG_CHUNK_OFFSET = 0xE0, // ..E3
	REG_CHUNK_LEN = 0xE4,    // E4
	REG_CHUNK_CRC16 = 0xE6,  // E6..E7
	// Aux debug (per other AI):
	REG_DBG_CRC16_CALC = 0xE8, // E8..E9 little-endian computed-by-aux
	REG_DBG_CRC16_HOST = 0xEA, // EA..EB little-endian host-sent (echo)
	REG_DATA = 0xC0,         // 32 bytes window
};

	// aux "GO" register: host can write 0x01 to jump to new app after finalize
	#define REG_GO 0xF9
	// Event flags reg (bit0 = DONE)
	#define REG_EVENT_FLAGS 0xED
	#define EVT_DONE_MASK   0x01

// Commands
enum {
	CMD_ENTER_READY = 0xA0,
	CMD_COMMIT_CHUNK = 0xB1,
	CMD_FINALIZE = 0xA2,
	CMD_SHUTDOWN = 0xA3,
};

// Status
enum {
	ST_IDLE = 0x00,
	ST_READY = 0x10,
	ST_BUSY = 0x11,
	ST_ERROR = 0x40,
};

// Error codes
enum {
	ERR_NONE = 0x00,
	ERR_BAD_PARAM = 0x01,
	ERR_CRC_MISMATCH = 0x02,
	ERR_FLASH_ERASE = 0x03,
	ERR_FLASH_PROG = 0x04,
};

// Timeouts and retries
#define READY_TIMEOUT_MS 50
#define OP_TIMEOUT_MS 150
#define RETRIES 2
// Max time to wait for a COMMIT to complete
// Worst-case first COMMIT may include multi-page flash erase on the aux MCU;
// allow generous time to avoid false timeouts on slower chips.
#define COMMIT_TIMEOUT_MS 5000
// Finalization takes longer (can verify, relocate, and restart the aux MCU)
// The overlay already warns that this can take up to 1–2 minutes.
#define FINALIZE_TIMEOUT_MS 120000

typedef struct {
	Gui* gui;
	ViewDispatcher* vd;
	TextBox* tbox;
	DialogsApp* dialogs;
	Storage* storage;
	NotificationApp* notifications;
	FuriString* logbuf;
	FuriThread* worker;
	volatile bool done;
	volatile bool success;
	char sel_path[256];
	// Confirm view pieces
	View* confirm_view;
	FuriTimer* confirm_timer;
	// Overlay view shown briefly after OK
	View* overlay_view;
	FuriTimer* overlay_timer;
	uint8_t overlay_phase; // 0=start worker soon, 1=switch back to verbose
	// Home view
	View* home_view;
	// Error view
	View* error_view;
} App;

// Model for confirmation view
typedef struct {
	uint8_t remaining; // seconds left to enable OK
	bool enabled;      // true when remaining == 0
} ConfirmModel;

static void confirm_timer_cb(void* ctx);
static void confirm_draw(Canvas* canvas, void* ctx);
static bool confirm_input(InputEvent* event, void* ctx);
static void confirm_enter(void* ctx);
static void confirm_exit(void* ctx);

// Overlay (flashing notice) view
static void overlay_draw(Canvas* canvas, void* ctx);
static void overlay_enter(void* ctx);
static void overlay_exit(void* ctx);
static void overlay_timer_cb(void* ctx);

// Home view
static void home_draw(Canvas* canvas, void* ctx);
static bool home_input(InputEvent* event, void* ctx);

// Error view
typedef struct {
	char line1[32];
	char line2[32];
	char line3[32];
} ErrorModel;
static void error_draw(Canvas* canvas, void* ctx);
static bool error_input(InputEvent* event, void* ctx);
static uint32_t on_prev_to_home(void* ctx);
static void show_error(App* app, const char* l1, const char* l2, const char* l3);

static void klog(App* app, const char* fmt, ...) {
	if(!app || !app->tbox) return;
	char line[160];
	va_list args;
	va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);

	// Append with newline and clamp total size
	furi_string_cat_printf(app->logbuf, "%s\r\n", line);
	if(furi_string_size(app->logbuf) > 4096) {
		// Drop oldest half to keep it responsive
		size_t drop = furi_string_size(app->logbuf) / 2;
		furi_string_right(app->logbuf, drop);
	}
	text_box_set_text(app->tbox, furi_string_get_cstr(app->logbuf));
	text_box_set_focus(app->tbox, TextBoxFocusEnd);
}

static bool i2c_write_mem_retry(const FuriHalI2cBusHandle* bus, uint8_t addr, uint8_t reg,
								const uint8_t* data, size_t len, uint32_t timeout) {
	for(int i = 0; i <= RETRIES; i++) {
		if(furi_hal_i2c_write_mem(bus, addr, reg, data, len, timeout)) return true;
		// Fallback: send [reg]+data in tx
		uint8_t buf[1 + 32];
		if(len <= 32) {
			buf[0] = reg;
			memcpy(buf + 1, data, len);
			if(furi_hal_i2c_tx(bus, addr, buf, len + 1, timeout)) return true;
		}
		furi_delay_ms(2);
	}
	return false;
}

static bool i2c_read_mem_restart(
	const FuriHalI2cBusHandle* bus,
	uint8_t addr,
	uint8_t reg,
	uint8_t* data,
	size_t len,
	uint32_t timeout) {
	// Write register, no STOP, then repeated START and read
	if(!furi_hal_i2c_tx_ext(
		   bus,
		   addr,
		   false,
		   &reg,
		   1,
		   FuriHalI2cBeginStart,
		   FuriHalI2cEndAwaitRestart,
		   timeout))
		return false;
	return furi_hal_i2c_rx_ext(
		bus, addr, false, data, len, FuriHalI2cBeginRestart, FuriHalI2cEndStop, timeout);
}

static bool i2c_read_mem_retry(const FuriHalI2cBusHandle* bus, uint8_t addr, uint8_t reg,
							   uint8_t* data, size_t len, uint32_t timeout) {
	for(int i = 0; i <= RETRIES; i++) {
		// Preferred: repeated-start
		if(i2c_read_mem_restart(bus, addr, reg, data, len, timeout)) return true;
		// Fallback: STOP + START
		if(furi_hal_i2c_trx(bus, addr, &reg, 1, data, len, timeout)) return true;
		furi_delay_ms(2);
	}
	return false;
}

static bool updater_is_ready(const FuriHalI2cBusHandle* bus) {
	// Must be called while bus acquired
	return furi_hal_i2c_is_device_ready(bus, KIISU_UPD_ADDR, READY_TIMEOUT_MS);
}

static bool updater_write_u32(const FuriHalI2cBusHandle* bus, uint8_t base, uint32_t val) {
	uint8_t le[4];
	bit_lib_num_to_bytes_le(val, 4, le);
	return i2c_write_mem_retry(bus, KIISU_UPD_ADDR, base, le, 4, OP_TIMEOUT_MS);
}

static bool updater_read_u8(const FuriHalI2cBusHandle* bus, uint8_t reg, uint8_t* out) {
	return i2c_read_mem_retry(bus, KIISU_UPD_ADDR, reg, out, 1, OP_TIMEOUT_MS);
}

static bool updater_write_u16(const FuriHalI2cBusHandle* bus, uint8_t base, uint16_t val) {
	uint8_t le[2];
	bit_lib_num_to_bytes_le(val, 2, le);
	return i2c_write_mem_retry(bus, KIISU_UPD_ADDR, base, le, 2, OP_TIMEOUT_MS);
}

static bool updater_cmd(const FuriHalI2cBusHandle* bus, uint8_t cmd) {
	return i2c_write_mem_retry(bus, KIISU_UPD_ADDR, REG_CMD, &cmd, 1, OP_TIMEOUT_MS);
}

static uint16_t crc16_x25(const uint8_t* data, size_t len) {
	return bit_lib_crc16(data, len, 0x1021, 0xFFFF, true, true, 0xFFFF);
}

// CRC32 (IEEE) incremental implementation matching aux crc32_step
static uint32_t crc32_step_host(uint32_t crc, const uint8_t* data, size_t len) {
	crc = ~crc;
	for (size_t i = 0; i < len; ++i) {
		crc ^= data[i];
		for (uint8_t b = 0; b < 8; ++b) {
			crc = (crc >> 1) ^ (0xEDB88320U & (-(int32_t)(crc & 1)));
		}
	}
	return ~crc;
}

static bool ensure_otg_power(void) {
	if(furi_hal_power_is_otg_enabled()) return true;
	return furi_hal_power_enable_otg();
}

static void force_i2c_weak_pullups(bool enable) {
	// Force weak pull-ups on PA9/PA10 when idle to help stability
	if(enable) {
		furi_hal_gpio_init_ex(
			&gpio_i2c_power_sda, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C1);
		furi_hal_gpio_init_ex(
			&gpio_i2c_power_scl, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C1);
	} else {
		// Back to default open-drain without pull
		furi_hal_gpio_init_ex(
			&gpio_i2c_power_sda, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C1);
		furi_hal_gpio_init_ex(
			&gpio_i2c_power_scl, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C1);
	}
}

static bool perform_update(App* app, const char* filepath, FuriString* out_msg) {
	bool ok = false;
	File* file = storage_file_alloc(app->storage);
	klog(app, "Opening file: %s", filepath);
	if(!storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
		furi_string_set(out_msg, "Failed to open file");
	klog(app, "ERROR: open failed");
		goto cleanup;
	}

	uint64_t size64 = storage_file_size(file);
	if(size64 == 0 || size64 > 0x0000C000 /* ~48KB */) {
		furi_string_set(out_msg, "Invalid size (max ~48KB)");
	klog(app, "ERROR: invalid size %lu", (unsigned long)size64);
		goto cleanup;
	}
	const uint32_t total_len = (uint32_t)size64;
	klog(app, "Total length: %lu bytes", (unsigned long)total_len);

	klog(app, "Enabling OTG power...");
	if(!ensure_otg_power()) {
		furi_string_set(out_msg, "OTG power failed");
	klog(app, "ERROR: OTG enable failed");
		goto cleanup;
	}
	klog(app, "OTG power ON");

	// Acquire power I2C bus and run the protocol
	klog(app, "Acquiring power I2C bus...");
	furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
	force_i2c_weak_pullups(true);
	klog(app, "Applied weak pull-ups on PA9/PA10");

	// Probe while acquired
	klog(app, "Probing device @ 0x%02X (7-bit)", KIISU_UPD_ADDR_7B);
	if(!updater_is_ready(&furi_hal_i2c_handle_power)) {
		furi_string_set(out_msg, "Device not found (0x30)");
	klog(app, "ERROR: not responding");
		goto release_bus;
	}
	klog(app, "Device responded");

	// Write total length
	klog(app, "Writing TOTAL_LEN...");
	if(!updater_write_u32(&furi_hal_i2c_handle_power, REG_TOTAL_LEN, total_len)) {
		furi_string_set(out_msg, "Write TOTAL_LEN failed");
	klog(app, "ERROR: TOTAL_LEN write failed");
		goto release_bus;
	}

	// Enter ready
	klog(app, "Sending ENTER_READY (0xA0)...");
	if(!updater_cmd(&furi_hal_i2c_handle_power, CMD_ENTER_READY)) {
		furi_string_set(out_msg, "ENTER_READY failed");
	klog(app, "ERROR: ENTER_READY failed");
		goto release_bus;
	}
	// Wait for READY
	{
		uint8_t st = 0xFF; uint32_t tries = 50; // ~50*2ms=100ms typical
		do {
			if(!updater_read_u8(&furi_hal_i2c_handle_power, REG_STATUS, &st)) break;
			if(st == ST_READY) break;
			furi_delay_ms(2);
		} while(--tries);
		if(st != ST_READY) {
			furi_string_set(out_msg, "Not READY");
			klog(app, "ERROR: STATUS=0x%02X not READY", st);
			goto release_bus;
		}
	}
	klog(app, "STATUS=READY");

	// Transfer chunks
	uint8_t buf[32];
	uint32_t offset = 0;
	uint32_t rolling_crc32 = 0;
	// Host-side running CRC32 for verification
	uint32_t host_rolling_crc32 = 0;
	while(offset < total_len) {
		size_t rem = total_len - offset;
		size_t len = rem > sizeof(buf) ? sizeof(buf) : rem;
		// Align to 8 bytes when possible (except last short remainder)
		if(len > 8) {
			size_t aligned = (len / 8) * 8;
			if(aligned == 0) aligned = 8;
			len = aligned;
		}
	klog(app, "Chunk @0x%08lX len=%lu", (unsigned long)offset, (unsigned long)len);
		size_t rd = storage_file_read(file, buf, len);
		if(rd != len) { furi_string_set(out_msg, "Read error"); goto release_bus; }
		uint8_t orig_len = (uint8_t)len;

		// If this is the very first chunk, validate the vector table of the file
		if(offset == 0 && len >= 8) {
			const uint32_t STAGE_BASE = 0x08010000UL; // must match aux NEW_APP_BASE
			const uint32_t BASE_ADDR  = 0x08000000UL;
			uint32_t new_sp = (uint32_t)bit_lib_bytes_to_num_le(buf, 4);
			uint32_t new_reset = (uint32_t)bit_lib_bytes_to_num_le(buf + 4, 4);
			// SP should point into SRAM (0x2000xxxx on STM32G4)
			bool sp_ok = ((new_sp & 0xFF000000u) == 0x20000000u);
			// Accept images linked for either 0x08000000 (BASE) or 0x08010000 (STAGE)
			bool reset_in_stage = (new_reset >= STAGE_BASE) && (new_reset < (STAGE_BASE + total_len));
			bool reset_in_base  = (new_reset >= BASE_ADDR)  && (new_reset < (BASE_ADDR + total_len));
			if(!sp_ok || (!reset_in_stage && !reset_in_base)) {
				klog(app, "Vector check failed: SP=0x%08lX (RAM=%s) RESET=0x%08lX (in base|stage=%s)",
					(unsigned long)new_sp, sp_ok?"yes":"no",
					(unsigned long)new_reset, (reset_in_stage||reset_in_base)?"yes":"no");
				furi_string_set(out_msg, "Bad vector table");
				goto release_bus;
			}
			klog(app, "Vector OK: %s-linked image", reset_in_stage?"staged(0x08010000)":"base(0x08000000)");
		}

		// Calc checksum
		uint16_t c16 = crc16_x25(buf, len);
	klog(app, "CRC16(X25)=0x%04X", c16);

		// Program registers
		if(!updater_write_u32(&furi_hal_i2c_handle_power, REG_CHUNK_OFFSET, offset)) {
			furi_string_set(out_msg, "Write OFFSET failed"); goto release_bus;
		}
	furi_delay_ms(1);
		{
			uint8_t l8 = (uint8_t)len;
			if(!i2c_write_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_CHUNK_LEN, &l8, 1, OP_TIMEOUT_MS)) {
				furi_string_set(out_msg, "Write LEN failed"); goto release_bus;
			}
		}
	furi_delay_ms(1);
		if(!updater_write_u16(&furi_hal_i2c_handle_power, REG_CHUNK_CRC16, c16)) {
			furi_string_set(out_msg, "Write CRC16 failed"); goto release_bus;
		}
	furi_delay_ms(1);
		if(!i2c_write_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DATA, buf, len, OP_TIMEOUT_MS)) {
			furi_string_set(out_msg, "Write DATA failed"); goto release_bus;
		}
		// Optional: verify that aux latched what we wrote (helps diagnose READY-stuck)
		{
			uint8_t rb4[4]; uint8_t rblen = 0; uint8_t rbcrc[2]; uint8_t rbdata[8];
			if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_CHUNK_OFFSET, rb4, 4, OP_TIMEOUT_MS)) {
				uint32_t off_rb = (uint32_t)bit_lib_bytes_to_num_le(rb4, 4);
				if(off_rb != offset) klog(app, "WARN: OFFSET rb=0x%08lX != 0x%08lX", (unsigned long)off_rb, (unsigned long)offset);
			}
			if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_CHUNK_LEN, &rblen, 1, OP_TIMEOUT_MS)) {
				if(rblen != (uint8_t)len) klog(app, "WARN: LEN rb=%u != %lu", rblen, (unsigned long)len);
			}
			if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_CHUNK_CRC16, rbcrc, 2, OP_TIMEOUT_MS)) {
				uint16_t c_rb = (uint16_t)((uint16_t)rbcrc[0] | ((uint16_t)rbcrc[1] << 8));
				if(c_rb != c16) klog(app, "WARN: CRC16 rb=0x%04X != 0x%04X", c_rb, c16);
			}
			size_t peek = len < sizeof(rbdata) ? len : sizeof(rbdata);
			if(peek && i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DATA, rbdata, peek, OP_TIMEOUT_MS)) {
				if(memcmp(rbdata, buf, peek) != 0) klog(app, "WARN: DATA[0..%u) rb mismatch", (unsigned)peek);
			}
		}
		// Small settle delay before COMMIT in case ISR needs to process writes
		furi_delay_ms(1);
		// Commit
	klog(app, "COMMIT_CHUNK... (polling)");
		if(!updater_cmd(&furi_hal_i2c_handle_power, CMD_COMMIT_CHUNK)) {
			furi_string_set(out_msg, "COMMIT failed"); goto release_bus;
		}
		// Small settle window before polling; aux may set BUSY shortly after
		furi_delay_ms(10);
		// Best-effort peek (optional)
		{
			uint8_t st0 = 0xFF;
			if(updater_read_u8(&furi_hal_i2c_handle_power, REG_STATUS, &st0)) {
				klog(app, "STATUS after COMMIT=0x%02X", st0);
			}
		}
		// Poll status to READY, handle ERROR immediately and verify programming by reading back RX_LEN + ROLL_CRC32
	uint8_t st = 0xFF; uint8_t prev_st = 0xFE; uint8_t err = 0;
	uint32_t polls = COMMIT_TIMEOUT_MS / 5; // 5ms steps
	uint32_t steps_since_commit = 0;
		uint32_t consec_read_fail = 0;
		do {
			if(!updater_read_u8(&furi_hal_i2c_handle_power, REG_STATUS, &st)) {
				consec_read_fail++;
				if((consec_read_fail % 40) == 0) klog(app, "STATUS read fail x%lu", (unsigned long)consec_read_fail);
				furi_delay_ms(5);
				steps_since_commit++;
				continue;
			}
			consec_read_fail = 0;
			if(st != prev_st) {
				if(st == ST_BUSY) klog(app, "STATUS=BUSY");
				else if(st == ST_READY) klog(app, "STATUS=READY");
				else if(st == ST_ERROR) klog(app, "STATUS=ERROR");
				else klog(app, "STATUS=0x%02X", st);
				prev_st = st;
			}
			if(st == ST_READY) break;
			if(st == ST_ERROR) {
				// Read aux ERROR & debug CRCs and abort immediately
				updater_read_u8(&furi_hal_i2c_handle_power, REG_ERROR, &err);
				uint8_t calc_le[2] = {0}; uint8_t host_le[2] = {0};
				i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DBG_CRC16_CALC, calc_le, 2, OP_TIMEOUT_MS);
				i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DBG_CRC16_HOST, host_le, 2, OP_TIMEOUT_MS);
				uint16_t calc = (uint16_t)((uint16_t)calc_le[0] | ((uint16_t)calc_le[1] << 8));
				uint16_t hostcrc = (uint16_t)((uint16_t)host_le[0] | ((uint16_t)host_le[1] << 8));
				klog(app, "STATUS=ERROR (0x%02X). AUX CRC calc=0x%04X host=0x%04X our=0x%04X", err, calc, hostcrc, c16);
				furi_string_set(out_msg, "Aux reported CRC/commit error");
				goto release_bus;
			}
			furi_delay_ms(5);
			steps_since_commit++;
		} while(--polls);
		if(st != ST_READY) {
			klog(app, "FINAL STATUS after commit != READY (0x%02X)", st);
			furi_string_set(out_msg, "Commit timeout");
			goto release_bus;
		}
		klog(app, "STATUS=READY");

		// Verify aux recorded RX_LEN and ROLL_CRC32 match host expectations
		{
			uint8_t rxlen_le[4]; uint8_t crc32le[4];
				if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_RECV_LEN, rxlen_le, 4, OP_TIMEOUT_MS) &&
					i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_ROLL_CRC32, crc32le, 4, OP_TIMEOUT_MS)) {
				uint32_t rxlen_rb = (uint32_t)bit_lib_bytes_to_num_le(rxlen_le, 4);
				uint32_t crc32_rb = (uint32_t)bit_lib_bytes_to_num_le(crc32le, 4);
				// Update host rolling CRC32 and expected rxlen
					 host_rolling_crc32 = crc32_step_host(host_rolling_crc32, buf, (size_t)orig_len);
					 uint32_t expected_rx = offset + (uint32_t)orig_len;
				if(rxlen_rb != expected_rx || crc32_rb != host_rolling_crc32) {
					klog(app, "VERIFY FAIL: aux RX_LEN=0x%08lX CRC32=0x%08lX != expected RX_LEN=0x%08lX CRC32=0x%08lX",
						(unsigned long)rxlen_rb, (unsigned long)crc32_rb, (unsigned long)expected_rx, (unsigned long)host_rolling_crc32);
					furi_string_set(out_msg, "Verify mismatch after commit");
					goto release_bus;
				}
				klog(app, "VERIFY OK: RX_LEN=0x%08lX CRC32=0x%08lX", (unsigned long)rxlen_rb, (unsigned long)crc32_rb);
			} else {
				klog(app, "WARN: could not read RX_LEN/ROLL_CRC32 for verify");
			}
		}

		// Optional: read rolling CRC32
		uint8_t crc32le[4];
		if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_ROLL_CRC32, crc32le, 4, OP_TIMEOUT_MS)) {
			rolling_crc32 = (uint32_t)bit_lib_bytes_to_num_le(crc32le, 4);
			klog(app, "Rolling CRC32=0x%08lX", (unsigned long)rolling_crc32);
		}

		offset += len;
	}

	// Finalize
	klog(app, "Sending FINALIZE (0xA2). Jump will occur when safe...");
	// Double-check that aux received length equals our file size before finalize
	{
		uint8_t rxlen_le[4];
		if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_RECV_LEN, rxlen_le, 4, OP_TIMEOUT_MS)) {
			uint32_t rxlen_rb = (uint32_t)bit_lib_bytes_to_num_le(rxlen_le, 4);
			if(rxlen_rb != total_len) {
				klog(app, "Pre-FINALIZE RX_LEN mismatch: aux=0x%08lX expected=0x%08lX", (unsigned long)rxlen_rb, (unsigned long)total_len);
				furi_string_set(out_msg, "Aux length mismatch before finalize");
				goto release_bus;
			}
		}
	}
	if(!updater_cmd(&furi_hal_i2c_handle_power, CMD_FINALIZE)) {
		furi_string_set(out_msg, "FINALIZE failed");
	klog(app, "ERROR: FINALIZE failed");
		goto release_bus;
	}
	// Do not write REG_GO; aux will autonomously jump/shutdown after finalize
	// Wait for the aux to complete FINALIZE; accept STATUS=READY, DONE flag, or device disappearance (aux reboot)
	{
		klog(app, "Polling STATUS/DONE after FINALIZE (up to %u ms)...", (unsigned)FINALIZE_TIMEOUT_MS);
		uint8_t st = 0xFF; uint8_t prev_st = 0xFE; uint32_t polls = FINALIZE_TIMEOUT_MS / 20; // 20ms steps
		bool done_seen = false; uint32_t not_ready_count = 0;
		do {
			// Check if device still responds on the bus
			if(!furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, READY_TIMEOUT_MS)) {
				not_ready_count++;
				if((not_ready_count % 25) == 0) klog(app, "Device not responding during finalize (likely rebooting)...");
				// Consider disappearance after finalize as success signal (aux rebooted)
				if(not_ready_count > 50) { // ~1s of absence
					klog(app, "Device disappeared after finalize; treating as success");
					done_seen = true;
					break;
				}
			} else {
				not_ready_count = 0;
				// Read STATUS best-effort
				if(updater_read_u8(&furi_hal_i2c_handle_power, REG_STATUS, &st)) {
					if(st != prev_st) {
						if(st == ST_BUSY) klog(app, "STATUS=BUSY");
						else if(st == ST_READY) klog(app, "STATUS=READY");
						else if(st == ST_ERROR) klog(app, "STATUS=ERROR");
						else klog(app, "STATUS=0x%02X", st);
						prev_st = st;
					}
					if(st == ST_READY || st == ST_ERROR) break;
				}
				// Also poll DONE flag; if asserted, treat as success path and break out
				uint8_t flags = 0;
				if(i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_EVENT_FLAGS, &flags, 1, OP_TIMEOUT_MS)) {
					if(flags & EVT_DONE_MASK) { done_seen = true; break; }
				}
			}
			furi_delay_ms(20);
		} while(--polls);
		if(!(st == ST_READY || done_seen)) {
			uint8_t err_fin = 0xFF; uint8_t dbg_calc[2] = {0}, dbg_host[2] = {0}; uint8_t dbg_reset[4] = {0};
			updater_read_u8(&furi_hal_i2c_handle_power, REG_ERROR, &err_fin);
			i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DBG_CRC16_CALC, dbg_calc, 2, OP_TIMEOUT_MS);
			i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, REG_DBG_CRC16_HOST, dbg_host, 2, OP_TIMEOUT_MS);
			i2c_read_mem_retry(&furi_hal_i2c_handle_power, KIISU_UPD_ADDR, 0xEC /* REG_DBG_VECT_RESET_0 */, dbg_reset, 4, OP_TIMEOUT_MS);
			uint16_t calc = (uint16_t)(dbg_calc[0] | ((uint16_t)dbg_calc[1] << 8));
			uint16_t host = (uint16_t)(dbg_host[0] | ((uint16_t)dbg_host[1] << 8));
			uint32_t staged_reset = (uint32_t)bit_lib_bytes_to_num_le(dbg_reset, 4);
			klog(app, "FINALIZE timeout: STATUS=0x%02X ERROR=0x%02X DBG calc=0x%04X host=0x%04X staged_reset=0x%08lX",
				st, err_fin, calc, host, (unsigned long)staged_reset);
			furi_string_set(out_msg, "Finalize timeout on aux");
			goto release_bus;
		}
		if(st == ST_READY) klog(app, "FINALIZE completed: STATUS=READY");
		else klog(app, "FINALIZE completed: DONE asserted or reboot detected");

		// Play a success notification and give it a moment before aux powers off/reboots
		if(app->notifications) notification_message(app->notifications, &sequence_success);
		furi_delay_ms(2000);
		// Aux may self-shutdown shortly after success; no more host actions needed here
	}

	furi_string_printf(out_msg, "Flashed %lu bytes. CRC32=0x%08lX", (unsigned long)total_len, (unsigned long)rolling_crc32);
	klog(app, "DONE: %s", furi_string_get_cstr(out_msg));
	ok = true;

release_bus:
	force_i2c_weak_pullups(false);
	furi_hal_i2c_release(&furi_hal_i2c_handle_power);
	klog(app, "Released I2C bus and restored pulls");
cleanup:
	storage_file_close(file);
	storage_file_free(file);
	return ok;
}

static uint32_t on_prev(void* ctx) {
	UNUSED(ctx);
	// Allow exit anytime; logs remain until Back is pressed
	return VIEW_NONE;
}

static int32_t updater_thread(void* ctx) {
	App* app = ctx;
	FuriString* msg = furi_string_alloc();
	app->success = perform_update(app, app->sel_path, msg);
	if(!app->success) {
		klog(app, "FAILED: %s", furi_string_get_cstr(msg));
	}
	klog(app, "Press BACK to exit");
	furi_string_free(msg);
	app->done = true;
	return 0;
}

int32_t kiisu_companion_bridge_app(void* arg) {
	UNUSED(arg);
	App app = {0};
	app.gui = furi_record_open(RECORD_GUI);
	app.dialogs = furi_record_open(RECORD_DIALOGS);
	app.storage = furi_record_open(RECORD_STORAGE);
	app.notifications = furi_record_open(RECORD_NOTIFICATION);

	app.vd = view_dispatcher_alloc();
	view_dispatcher_attach_to_gui(app.vd, app.gui, ViewDispatcherTypeFullscreen);
	app.tbox = text_box_alloc();
	app.logbuf = furi_string_alloc();
	view_dispatcher_add_view(app.vd, 0, text_box_get_view(app.tbox));
	view_set_previous_callback(text_box_get_view(app.tbox), on_prev);

	// Home view (ID 3)
	app.home_view = view_alloc();
	view_set_context(app.home_view, &app);
	view_set_draw_callback(app.home_view, home_draw);
	view_set_input_callback(app.home_view, home_input);
	view_set_previous_callback(app.home_view, on_prev);
	view_dispatcher_add_view(app.vd, 3, app.home_view);
	// Prepare confirmation view (ID 1)
	app.confirm_view = view_alloc();
	view_allocate_model(app.confirm_view, ViewModelTypeLocking, sizeof(ConfirmModel));
	view_set_context(app.confirm_view, &app);
	view_set_draw_callback(app.confirm_view, confirm_draw);
	view_set_input_callback(app.confirm_view, confirm_input);
	view_set_enter_callback(app.confirm_view, confirm_enter);
	view_set_exit_callback(app.confirm_view, confirm_exit);
	view_set_previous_callback(app.confirm_view, on_prev);
	view_dispatcher_add_view(app.vd, 1, app.confirm_view);
	// Timer for countdown
	app.confirm_timer = furi_timer_alloc(confirm_timer_cb, FuriTimerTypePeriodic, &app);

	// Prepare overlay (ID 2)
	app.overlay_view = view_alloc();
	view_set_context(app.overlay_view, &app);
	view_set_draw_callback(app.overlay_view, overlay_draw);
	view_set_enter_callback(app.overlay_view, overlay_enter);
	view_set_exit_callback(app.overlay_view, overlay_exit);
	view_set_previous_callback(app.overlay_view, on_prev);
	view_dispatcher_add_view(app.vd, 2, app.overlay_view);
	app.overlay_timer = furi_timer_alloc(overlay_timer_cb, FuriTimerTypeOnce, &app);

	// Show home screen first
	view_dispatcher_switch_to_view(app.vd, 3);

	// Error view (ID 4)
	app.error_view = view_alloc();
	view_allocate_model(app.error_view, ViewModelTypeLocking, sizeof(ErrorModel));
	view_set_context(app.error_view, &app);
	view_set_draw_callback(app.error_view, error_draw);
	view_set_input_callback(app.error_view, error_input);
	view_set_previous_callback(app.error_view, on_prev_to_home);
	view_dispatcher_add_view(app.vd, 4, app.error_view);

	// Run UI until Back
	view_dispatcher_run(app.vd);
	// Join worker if still running
	if(app.worker) {
		furi_thread_join(app.worker);
		furi_thread_free(app.worker);
	}
	// Cleanup confirm view
	if(app.confirm_timer) {
		furi_timer_free(app.confirm_timer);
		app.confirm_timer = NULL;
	}
	if(app.confirm_view) {
		view_dispatcher_remove_view(app.vd, 1);
		view_free(app.confirm_view);
		app.confirm_view = NULL;
	}
	if(app.home_view) {
		view_dispatcher_remove_view(app.vd, 3);
		view_free(app.home_view);
		app.home_view = NULL;
	}
	// Cleanup overlay view
	if(app.overlay_timer) {
		furi_timer_free(app.overlay_timer);
		app.overlay_timer = NULL;
	}
	if(app.overlay_view) {
		view_dispatcher_remove_view(app.vd, 2);
		view_free(app.overlay_view);
		app.overlay_view = NULL;
	}
	if(app.error_view) {
		view_dispatcher_remove_view(app.vd, 4);
		view_free(app.error_view);
		app.error_view = NULL;
	}
	if(app.tbox) {
		view_dispatcher_remove_view(app.vd, 0);
		text_box_free(app.tbox);
	}
	if(app.logbuf) furi_string_free(app.logbuf);
	view_dispatcher_free(app.vd);
	furi_record_close(RECORD_STORAGE);
	furi_record_close(RECORD_DIALOGS);
	if(app.notifications) furi_record_close(RECORD_NOTIFICATION);
	furi_record_close(RECORD_GUI);
	return 0;
}

// ===== Confirmation View Implementation =====

static void confirm_timer_cb(void* ctx) {
	App* app = ctx;
	if(!app || !app->confirm_view) return;
	ConfirmModel* model = view_get_model(app->confirm_view);
	if(model->enabled) {
		view_commit_model(app->confirm_view, false);
		return;
	}
	if(model->remaining > 0) {
		model->remaining--;
		if(model->remaining == 0) model->enabled = true;
	}
	view_commit_model(app->confirm_view, true);
}

static void draw_warning_layers(Canvas* canvas) {
	// Layer 1
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 38, 8, "WARNING");

	// Layer 2 copy
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str(canvas, 7, 26, "YOUR OLD AUX FIRMWARE");

	// Layer 2
	canvas_draw_str(canvas, 15, 17, "THIS WILL OVERWRITE");

	// Layer 4 copy
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 5, 51, "IF KIISU GETS BRICKED");

	// Layer 4
	canvas_draw_str(canvas, 13, 40, "NOT OUR PROBLEM");

	// Layer 6
	canvas_draw_line(canvas, 0, 28, 127, 28);
}

static void confirm_draw(Canvas* canvas, void* ctx) {
	// ctx is the model for draw callbacks
	ConfirmModel* model = ctx;

	// Background
	canvas_clear(canvas);

	// Warning text layers
	draw_warning_layers(canvas);

	// Center button with countdown text
	char label[8];
	if(model->enabled) {
		elements_button_center(canvas, "Ok");
	} else {
		snprintf(label, sizeof(label), "(%u)", (unsigned)model->remaining);
		elements_button_center(canvas, label);
	}
}

static bool confirm_input(InputEvent* event, void* ctx) {
	App* app = ctx;
	furi_assert(app);
	bool consumed = false;
	if(event->type == InputTypeShort && event->key == InputKeyOk) {
		ConfirmModel* model = view_get_model(app->confirm_view);
		bool can_continue = model->enabled;
		view_commit_model(app->confirm_view, false);
		if(can_continue) {
			// Show flashing overlay first, then start worker and return to verbose
			app->overlay_phase = 0;
			view_dispatcher_switch_to_view(app->vd, 2);
			// Schedule worker start after a brief delay
			if(app->overlay_timer) furi_timer_start(app->overlay_timer, 50);

			consumed = true;
		} else {
			// Ignore OK press until enabled
			consumed = true;
		}
	}
	return consumed;
}

static void confirm_enter(void* ctx) {
	App* app = ctx;
	furi_assert(app);
	ConfirmModel* model = view_get_model(app->confirm_view);
	model->remaining = 10;
	model->enabled = false;
	view_commit_model(app->confirm_view, true);
	if(app->confirm_timer) furi_timer_start(app->confirm_timer, 1000);
}

static void confirm_exit(void* ctx) {
	App* app = ctx;
	furi_assert(app);
	if(app->confirm_timer) furi_timer_stop(app->confirm_timer);
}

// ===== Overlay (Flashing notice) =====

static void overlay_draw(Canvas* canvas, void* ctx) {
	UNUSED(ctx);
	// Ensure background is cleared
	canvas_clear(canvas);

	// Layer 1
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 23, 9, "!!! FLASHING !!!");

	// Layer 5 copy
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str(canvas, 7, 28, "Unplug and replug if you see");

	// Layer 5 copy
	canvas_draw_str(canvas, 10, 48, "Disconnect and reconnect");

	// Layer 5 copy
	canvas_draw_str(canvas, 27, 56, "battery if you only");

	// Layer 5 copy
	canvas_draw_str(canvas, 26, 37, "green LED flashing");

	// Layer 5
	canvas_draw_str(canvas, 15, 17, "This might take 2-3min");

	// Layer 5 copy
	canvas_draw_str(canvas, 25, 64, "hear success sound");

	// Layer 8
	canvas_draw_line(canvas, 0, 19, 127, 19);

	// Layer 9
	canvas_draw_line(canvas, 0, 39, 127, 39);
}

static void overlay_enter(void* ctx) {
	App* app = ctx;
	furi_assert(app);
}

static void overlay_exit(void* ctx) {
	App* app = ctx;
	furi_assert(app);
}

static void overlay_timer_cb(void* ctx) {
	App* app = ctx;
	if(!app) return;
	if(app->overlay_phase == 0) {
		// Preflight: quick I2C compatibility check
		furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
		bool ok = updater_is_ready(&furi_hal_i2c_handle_power);
		furi_hal_i2c_release(&furi_hal_i2c_handle_power);
		if(!ok) {
			show_error(app, "COMMUNICATION ERROR", "Are you on a compatible", "AUX firmware?");
			return;
		}

		// Kick off worker after a short delay while overlay is visible
		text_box_set_text(app->tbox, "Kiisu Companion Bridge\r\nStarting...\r\n");
		text_box_set_focus(app->tbox, TextBoxFocusEnd);
		app->done = false;
		app->success = false;
		if(!app->worker) {
			app->worker = furi_thread_alloc_ex("kiisu_upd", 2048, updater_thread, app);
			furi_thread_start(app->worker);
		}
		app->overlay_phase = 1;
		// After a moment, switch back to verbose view
		furi_timer_start(app->overlay_timer, 300);
	} else if(app->overlay_phase == 1) {
		app->overlay_phase = 2;
		view_dispatcher_switch_to_view(app->vd, 0);
	}
}

// ===== Home (Landing) View =====

static void home_draw(Canvas* canvas, void* ctx) {
	UNUSED(ctx);
	canvas_clear(canvas);
	// Background image/logo
	canvas_draw_xbm(canvas, 0, 0, 128, 64, image_ST_michelangelo_0_bits);

	// Titles
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 7, 12, "Kiisu Companion");
	canvas_draw_str(canvas, 33, 22, "Bridge");

	// OS-style center button: show plug prompt if not charging
	const char* label = furi_hal_power_is_charging() ? "Select BIN file" : "Please plug in";
	elements_button_center(canvas, label);
}

static bool home_input(InputEvent* event, void* ctx) {
	App* app = ctx;
	if(event->type == InputTypeShort && event->key == InputKeyOk) {
		if(!furi_hal_power_is_charging()) {
			// Ignore OK if not charging
			return true;
		}
		// Open file picker at SD card root
	FuriString* path = furi_string_alloc_set(EXT_PATH(""));
		DialogsFileBrowserOptions opts;
		dialog_file_browser_set_basic_options(&opts, ".bin", NULL);
		if(dialog_file_browser_show(app->dialogs, path, path, &opts)) {
			// Save selection and go to confirm view
			strncpy(app->sel_path, furi_string_get_cstr(path), sizeof(app->sel_path) - 1);
			app->sel_path[sizeof(app->sel_path) - 1] = '\0';
			// Size safeguard: > 48 KiB
			File* f = storage_file_alloc(app->storage);
			uint64_t sz = 0;
			if(storage_file_open(f, app->sel_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
				sz = storage_file_size(f);
			}
			storage_file_close(f);
			storage_file_free(f);
			if(sz == 0 || sz > 0xC000ULL) {
				show_error(app, "FIRMWARE TOO LARGE", "Flash via usb", "or ST-LINK");
			} else {
				view_dispatcher_switch_to_view(app->vd, 1);
			}
		}
		furi_string_free(path);
		return true;
	}
	return false;
}

// ===== Error View =====

static void error_draw(Canvas* canvas, void* ctx) {
	ErrorModel* model = ctx;
	canvas_clear(canvas);
	// Center three lines; use FontPrimary for prominence
	canvas_set_font(canvas, FontPrimary);
	int16_t y1 = 18, y2 = 33, y3 = 48;
	int16_t x1 = (128 - canvas_string_width(canvas, model->line1)) / 2;
	int16_t x2 = (128 - canvas_string_width(canvas, model->line2)) / 2;
	int16_t x3 = (128 - canvas_string_width(canvas, model->line3)) / 2;
	canvas_draw_str(canvas, x1, y1, model->line1);
	canvas_draw_str(canvas, x2, y2, model->line2);
	canvas_draw_str(canvas, x3, y3, model->line3);

	elements_button_center(canvas, "Back");
}

static bool error_input(InputEvent* event, void* ctx) {
	App* app = ctx;
	if(event->type == InputTypeShort && event->key == InputKeyOk) {
		view_dispatcher_switch_to_view(app->vd, 3);
		return true;
	}
	return false;
}

static uint32_t on_prev_to_home(void* ctx) {
	UNUSED(ctx);
	return 3; // home view id
}

static void show_error(App* app, const char* l1, const char* l2, const char* l3) {
	ErrorModel* model = view_get_model(app->error_view);
	strncpy(model->line1, l1 ? l1 : "", sizeof(model->line1) - 1);
	model->line1[sizeof(model->line1) - 1] = '\0';
	strncpy(model->line2, l2 ? l2 : "", sizeof(model->line2) - 1);
	model->line2[sizeof(model->line2) - 1] = '\0';
	strncpy(model->line3, l3 ? l3 : "", sizeof(model->line3) - 1);
	model->line3[sizeof(model->line3) - 1] = '\0';
	view_commit_model(app->error_view, false);
	view_dispatcher_switch_to_view(app->vd, 4);
}

