#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>
#include <alt_boot.h>
#include <update_util/update_operation.h>

#define TAG "Main"

void dbg_beep(int n);

int32_t init_task(void* context) {
    UNUSED(context);

    // Flipper FURI HAL
    furi_hal_init();

    // Init flipper
    flipper_init();

    dbg_beep(2); /* 2 = full init complete */

    furi_background();

    return 0;
}

/* Debug beep on PB8 — N short beeps, clock-speed-aware */
void dbg_beep(int n) {
    volatile uint32_t* rcc = (volatile uint32_t*)(0x58000000UL + 0x4CUL);
    *rcc |= (1UL << 1); /* GPIOB clock */
    for(volatile int d = 0; d < 100; d++) {}
    volatile uint32_t* moder = (volatile uint32_t*)0x48000400UL;
    volatile uint32_t* bsrr = (volatile uint32_t*)0x48000418UL;
    *moder = (*moder & ~(3UL << 16)) | (1UL << 16);
    /* Scale delays based on CPU clock (4MHz=1, 64MHz=16) */
    uint32_t scale = SystemCoreClock / 4000000UL;
    if(scale == 0) scale = 1;
    uint32_t half = 500 * scale;
    uint32_t pause = 150000 * scale;
    uint32_t gap = 400000 * scale;
    for(int j = 0; j < n; j++) {
        for(int i = 0; i < 80; i++) {
            *bsrr = (1UL << 8);
            for(volatile uint32_t d = 0; d < half; d++) {}
            *bsrr = (1UL << 24);
            for(volatile uint32_t d = 0; d < half; d++) {}
        }
        for(volatile uint32_t d = 0; d < pause; d++) {} /* pause */
    }
    for(volatile uint32_t d = 0; d < gap; d++) {} /* gap after group */
}

int main(void) {
    dbg_beep(1); /* 1 = boot started */

    // Initialize FURI layer
    furi_init();

    // Flipper critical FURI HAL
    furi_hal_init_early();

    FuriThread* main_thread = furi_thread_alloc_ex("InitSrv", 1024, init_task, NULL);
    furi_thread_set_priority(main_thread, FuriThreadPriorityInit);

#ifdef FURI_RAM_EXEC
    // Prevent entering sleep mode when executed from RAM
    furi_hal_power_insomnia_enter();
    furi_thread_start(main_thread);
#else
    furi_hal_light_sequence("RGB");

    // Delay is for button sampling
    furi_delay_ms(100);

    FuriHalRtcBootMode boot_mode = furi_hal_rtc_get_boot_mode();
    if(boot_mode == FuriHalRtcBootModeDfu || !furi_hal_gpio_read(&gpio_button_left)) {
        furi_hal_light_sequence("rgb WB");
        furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeNormal);
        flipper_boot_dfu_exec();
        furi_hal_power_reset();
    } else if(boot_mode == FuriHalRtcBootModeUpdate) {
        furi_hal_light_sequence("rgb BR");
        // Do update
        flipper_boot_update_exec();
        // if things go nice, we shouldn't reach this point.
        // But if we do, abandon to avoid bootloops
        furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeNormal);
        furi_hal_power_reset();
    } else if(!furi_hal_gpio_read(&gpio_button_up)) {
        furi_hal_light_sequence("rgb WR");
        flipper_boot_recovery_exec();
        furi_hal_power_reset();
    } else {
        furi_hal_light_sequence("rgb G");
        furi_thread_start(main_thread);
    }
#endif

    // Run Kernel
    furi_run();

    furi_crash("Kernel is Dead");
}

void Error_Handler(void) {
    furi_crash("ErrorHandler");
}

void abort(void) {
    furi_crash("AbortHandler");
}
