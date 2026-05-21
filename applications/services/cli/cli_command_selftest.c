#include "cli_command_selftest.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>
#include <furi_hal_infrared.h>
#include <toolbox/cli/cli_command.h>
#include <toolbox/args.h>
#include <input/input.h>
#include <cc1101.h>
#include <lib/subghz/devices/cc1101_configs.h>

#define TAG "SelfTest"

#define I2C_TIMEOUT_MS 10

/* I2C addresses (8-bit, pre-shifted) */
#define BQ27220_I2C_ADDR 0xAA
#define BQ25896_I2C_ADDR 0xD6
#define LP5562_I2C_ADDR  0x60

/* IR test parameters */
#define IR_CARRIER_FREQ_HZ 38000UL
#define IR_CARRIER_DUTY    0.33f
#define IR_BURST_DURATION  600UL
#define IR_BURST_COUNT     50UL

typedef struct {
    bool level;
    uint32_t count;
} IrTxContext;

static uint32_t selftest_passed;
static uint32_t selftest_total;

static void selftest_report(uint32_t num, const char* name, bool ok, const char* detail) {
    printf("[%lu/%lu] %s: %s — %s\r\n", num, selftest_total, name, detail, ok ? "OK" : "FAIL");
    if(ok) selftest_passed++;
}

/* ---------- 1. I2C Bus Scan ---------- */

static void selftest_i2c_scan(uint32_t num) {
    const struct {
        const char* name;
        uint8_t addr;
    } devices[] = {
        {"BQ27220", BQ27220_I2C_ADDR},
        {"BQ25896", BQ25896_I2C_ADDR},
        {"LP5562", LP5562_I2C_ADDR},
    };

    bool all_ok = true;
    char detail[128];
    size_t pos = 0;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);

    for(size_t i = 0; i < COUNT_OF(devices); i++) {
        bool ready =
            furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_power, devices[i].addr, I2C_TIMEOUT_MS);
        int written = snprintf(
            detail + pos,
            sizeof(detail) - pos,
            "%s%s(0x%02X)=%s",
            (i > 0) ? " " : "",
            devices[i].name,
            devices[i].addr,
            ready ? "OK" : "FAIL");
        if(written > 0) pos += (size_t)written;
        if(!ready) all_ok = false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_power);

    selftest_report(num, "I2C Bus Scan", all_ok, detail);
}

/* ---------- 2. Sub-GHz CC1101 ---------- */

static void selftest_subghz(PipeSide* pipe, uint32_t num) {
    char detail[128];
    bool ok = false;

    /* Read chip ID */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint8_t partnumber = cc1101_get_partnumber(&furi_hal_spi_bus_handle_subghz);
    uint8_t version = cc1101_get_version(&furi_hal_spi_bus_handle_subghz);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(partnumber == 0xFF && version == 0xFF) {
        snprintf(
            detail,
            sizeof(detail),
            "Part=0x%02X, Ver=0x%02X (no SPI response)",
            partnumber,
            version);
        selftest_report(num, "Sub-GHz CC1101", false, detail);
        return;
    }

    /* TX test: load preset, set frequency, force CC1101 into TX mode bypassing region check */
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    uint32_t real_freq = cc1101_set_frequency(&furi_hal_spi_bus_handle_subghz, 433920000);
    cc1101_calibrate(&furi_hal_spi_bus_handle_subghz);
    furi_check(
        cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateIDLE, 10000));
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    furi_hal_subghz_set_path(FuriHalSubGhzPath433);

    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, true);

    /* Switch CC1101 to TX directly via SPI */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_switch_to_tx(&furi_hal_spi_bus_handle_subghz);
    ok = cc1101_wait_status_state(&furi_hal_spi_bus_handle_subghz, CC1101StateTX, 10000);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    if(ok) {
        printf("    TX carrier at %lu Hz...\r\n", real_freq);
        cli_sleep(pipe, 1000);
    }

    furi_hal_subghz_idle();
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_sleep();

    snprintf(
        detail,
        sizeof(detail),
        "Part=%d, Ver=%d, TX %lu.%02lu MHz %s",
        partnumber,
        version,
        real_freq / 1000000,
        (real_freq % 1000000) / 10000,
        ok ? "1s" : "FAIL");

    selftest_report(num, "Sub-GHz CC1101", ok, detail);
}

/* ---------- 3. NFC ST25R3916 ---------- */

static void selftest_nfc_chip(uint32_t num) {
    FuriHalNfcError err = furi_hal_nfc_is_hal_ready();
    char detail[64];
    snprintf(detail, sizeof(detail), "chip %s", (err == FuriHalNfcErrorNone) ? "ready" : "not ready");
    selftest_report(num, "NFC ST25R3916", err == FuriHalNfcErrorNone, detail);
}

/* ---------- 4. BLE Core2 ---------- */

static void selftest_ble(uint32_t num) {
    bool alive = furi_hal_bt_is_alive();
    selftest_report(num, "BLE Core2", alive, alive ? "alive" : "not responding");
}

/* ---------- 5. NFC Field ---------- */

static void selftest_nfc_field(PipeSide* pipe, uint32_t num) {
    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) {
        selftest_report(num, "NFC Field", false, "acquire failed (busy?)");
        return;
    }

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_poller_field_on();

    printf("    NFC field ON for 3s...\r\n");
    cli_sleep(pipe, 3000);

    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_release();

    selftest_report(num, "NFC Field", true, "ON 3s");
}

/* ---------- 6. RFID 125kHz ---------- */

static void selftest_rfid(PipeSide* pipe, uint32_t num) {
    furi_hal_rfid_tim_read_start(125000, 0.5);

    printf("    RFID carrier ON for 2s...\r\n");
    cli_sleep(pipe, 2000);

    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();

    selftest_report(num, "RFID 125kHz", true, "carrier 2s");
}

/* ---------- 7. IR TX ---------- */

static FuriHalInfraredTxGetDataState
    selftest_ir_tx_callback(void* context, uint32_t* duration, bool* level) {
    IrTxContext* ctx = context;

    *duration = IR_BURST_DURATION;
    *level = ctx->level;
    ctx->level = !ctx->level;
    ctx->count++;

    if(ctx->count < IR_BURST_COUNT * 2) {
        return FuriHalInfraredTxGetDataStateOk;
    } else {
        return FuriHalInfraredTxGetDataStateLastDone;
    }
}

static void selftest_ir(uint32_t num) {
    IrTxContext ctx = {.level = true, .count = 0};
    char detail[64];

    furi_hal_infrared_async_tx_set_data_isr_callback(selftest_ir_tx_callback, &ctx);
    furi_hal_infrared_async_tx_start(IR_CARRIER_FREQ_HZ, IR_CARRIER_DUTY);
    furi_hal_infrared_async_tx_wait_termination();

    snprintf(detail, sizeof(detail), "%lu bursts at %lu kHz", IR_BURST_COUNT, IR_CARRIER_FREQ_HZ / 1000);
    selftest_report(num, "IR TX", true, detail);
}

/* ---------- 8. Speaker ---------- */

static void selftest_speaker(PipeSide* pipe, uint32_t num) {
    if(!furi_hal_speaker_acquire(1000)) {
        selftest_report(num, "Speaker", false, "acquire failed (busy?)");
        return;
    }

    furi_hal_speaker_start(1000.0f, 0.5f);
    cli_sleep(pipe, 200);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();

    selftest_report(num, "Speaker", true, "beep 1kHz 200ms");
}

/* ---------- Main ---------- */

void cli_command_selftest(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    UNUSED(context);

    selftest_total = 8;
    selftest_passed = 0;

    printf("=== Kiisu Self-Test ===\r\n");

    selftest_i2c_scan(1);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_subghz(pipe, 2);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_nfc_chip(3);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_ble(4);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_nfc_field(pipe, 5);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_rfid(pipe, 6);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_ir(7);
    if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) return;

    selftest_speaker(pipe, 8);

    printf(
        "=== %lu/%lu passed ===%s\r\n",
        selftest_passed,
        selftest_total,
        (selftest_passed == selftest_total) ? "" : " (FAILURES DETECTED)");
}

/* ===================== PC-driven helpers ===================== */

/* Read optional positive-integer argument with default and ceiling. */
static uint32_t selftest_parse_timeout_s(FuriString* args, uint32_t default_s, uint32_t max_s) {
    if(!args || furi_string_size(args) == 0) return default_s;
    int parsed = 0;
    if(!args_read_int_and_trim(args, &parsed)) return default_s;
    if(parsed <= 0) return default_s;
    if((uint32_t)parsed > max_s) return max_s;
    return (uint32_t)parsed;
}

/* ---------- selftest_buttons ---------- */

static void selftest_buttons_input_cb(const void* value, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, value, 0);
}

void cli_command_selftest_buttons(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    uint32_t timeout_s = selftest_parse_timeout_s(args, 30, 300);

    const InputKey keys[] =
        {InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyOk, InputKeyBack};
    const char* names[] = {"Up", "Down", "Left", "Right", "Ok", "Back"};
    const size_t n_keys = COUNT_OF(keys);
    bool pressed[6] = {false};

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FuriPubSub* input_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* sub =
        furi_pubsub_subscribe(input_pubsub, selftest_buttons_input_cb, queue);

    printf("Press each button: Up, Down, Left, Right, Ok, Back (%lus)\r\n", timeout_s);

    uint32_t deadline = furi_get_tick() + timeout_s * 1000;
    bool all_done = false;

    while(furi_get_tick() < deadline) {
        if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) break;

        InputEvent event;
        if(furi_message_queue_get(queue, &event, 100) != FuriStatusOk) continue;
        if(event.type != InputTypeShort) continue;

        for(size_t i = 0; i < n_keys; i++) {
            if(event.key == keys[i] && !pressed[i]) {
                pressed[i] = true;
                printf("OK: %s\r\n", names[i]);
                break;
            }
        }

        all_done = true;
        for(size_t i = 0; i < n_keys; i++) {
            if(!pressed[i]) {
                all_done = false;
                break;
            }
        }
        if(all_done) break;
    }

    furi_pubsub_unsubscribe(input_pubsub, sub);
    furi_record_close(RECORD_INPUT_EVENTS);
    furi_message_queue_free(queue);

    if(all_done) {
        printf("Buttons: 6/6 OK\r\n");
    } else {
        printf("Buttons: FAIL — missing");
        for(size_t i = 0; i < n_keys; i++) {
            if(!pressed[i]) printf(" %s", names[i]);
        }
        printf("\r\n");
    }
}

/* ---------- selftest_nfc_card ---------- */

void cli_command_selftest_nfc_card(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    uint32_t timeout_s = selftest_parse_timeout_s(args, 5, 60);

    FuriHalNfcError err = furi_hal_nfc_acquire();
    if(err != FuriHalNfcErrorNone) {
        printf("NFC: acquire failed\r\n");
        return;
    }

    furi_hal_nfc_low_power_mode_stop();
    furi_hal_nfc_set_mode(FuriHalNfcModePoller, FuriHalNfcTechIso14443a);
    furi_hal_nfc_poller_field_on();

    printf("NFC field ON. Apply card within %lus\r\n", timeout_s);

    bool detected = false;
    uint8_t atqa[2] = {0};
    uint32_t deadline = furi_get_tick() + timeout_s * 1000;

    while(furi_get_tick() < deadline) {
        if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) break;

        furi_hal_nfc_trx_reset();
        if(furi_hal_nfc_iso14443a_poller_trx_short_frame(FuriHalNfcaShortFrameSensReq) !=
           FuriHalNfcErrorNone) {
            furi_delay_ms(50);
            continue;
        }
        /* Walk through the full TX → RX state machine. wait_event returns the
         * first event(s) that fire; accumulate until either RX_END (success)
         * or RX_TIMEOUT (no card in field) shows up. */
        FuriHalNfcEvent acc = 0;
        uint32_t wait_deadline = furi_get_tick() + 50;
        while(furi_get_tick() < wait_deadline) {
            FuriHalNfcEvent ev = furi_hal_nfc_poller_wait_event(10);
            acc |= ev;
            if(acc & (FuriHalNfcEventRxEnd | FuriHalNfcEventTimeout)) break;
        }
        if(acc & FuriHalNfcEventRxEnd) {
            size_t rx_bits = 0;
            if(furi_hal_nfc_poller_rx(atqa, sizeof(atqa), &rx_bits) ==
                   FuriHalNfcErrorNone &&
               rx_bits >= 16) {
                detected = true;
                break;
            }
        }
        furi_delay_ms(50);
    }

    /* low_power_mode_start switches the antenna off as part of entering LP. */
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_release();

    if(detected) {
        printf("NFC: OK ATQA=%02X%02X\r\n", atqa[1], atqa[0]);
    } else {
        printf("NFC: FAIL no card\r\n");
    }
}
