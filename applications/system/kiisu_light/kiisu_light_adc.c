/**
 * @file kiisu_light_adc.c
 * @brief Light level + flicker meter via the on-board ADC light sensor.
 *
 * Sampling the LGHT channel in a tight burst lets us estimate two things
 * commonly used to assess artificial light quality:
 *   - flicker depth Pf% = 100 * (max - min) / (max + min)  (IEEE 1789)
 *   - dominant ripple frequency, via zero-crossing of the centered signal
 */
#include <furi.h>
#include <furi_hal.h>

#include <core/core_defines.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>

/* DWT cycle counter for sub-microsecond burst timing */
#include <stm32wbxx.h>

#define LIGHT_CHANNEL    FuriHalAdcChannel4
#define LIGHT_FULL_SCALE 1187.0f /* mV at which the sensor saturates → 0% light */

#define BURST_SAMPLES 512u

typedef struct {
    float light_mv;
    float light_percent;
    float flicker_percent;
    float flicker_hz;
    float temp_c;
    bool flicker_valid;
} Reading;

static void
    burst_sample(FuriHalAdcHandle* adc, uint16_t* buf, size_t n, float* sample_rate_hz_out) {
    /* Tight loop. furi_hal_adc_read is synchronous; the resulting sample rate
     * is determined by ADC conversion time plus loop overhead. We measure it
     * with the DWT cycle counter so the frequency estimate is accurate even
     * if the loop speed varies between firmware revisions. */
    uint32_t cyc_start = DWT->CYCCNT;
    for(size_t i = 0; i < n; i++) {
        buf[i] = furi_hal_adc_read(adc, LIGHT_CHANNEL);
    }
    uint32_t cyc_delta = DWT->CYCCNT - cyc_start;
    float seconds = (float)cyc_delta / (float)SystemCoreClock;
    *sample_rate_hz_out = (seconds > 0.0f) ? ((float)n / seconds) : 0.0f;
}

static void
    analyze(const uint16_t* buf, size_t n, float sample_rate_hz, Reading* out) {
    uint16_t vmin = 0xFFFF, vmax = 0;
    uint64_t sum = 0;
    for(size_t i = 0; i < n; i++) {
        if(buf[i] < vmin) vmin = buf[i];
        if(buf[i] > vmax) vmax = buf[i];
        sum += buf[i];
    }
    float mean = (float)sum / (float)n;

    /* Flicker depth — IEEE 1789 "percent flicker" formula. Undefined when the
     * signal is essentially zero (sensor saturated dark). */
    uint32_t denom = (uint32_t)vmax + (uint32_t)vmin;
    if(denom > 0 && vmax > vmin) {
        out->flicker_percent = 100.0f * (float)(vmax - vmin) / (float)denom;
    } else {
        out->flicker_percent = 0.0f;
    }

    /* Dominant frequency via zero-crossings of the centered signal. Use a
     * small hysteresis around the mean (~5% of peak-to-peak) so wideband ADC
     * noise on a flat signal doesn't manufacture false crossings. */
    uint16_t pkpk = vmax - vmin;
    float hyst = (float)pkpk * 0.05f;
    if(hyst < 2.0f) hyst = 2.0f;

    int crossings = 0;
    int prev_sign = 0; /* 0 = unknown, +1 above, -1 below */
    for(size_t i = 0; i < n; i++) {
        int sign;
        if((float)buf[i] > mean + hyst) {
            sign = 1;
        } else if((float)buf[i] < mean - hyst) {
            sign = -1;
        } else {
            continue;
        }
        if(prev_sign != 0 && sign != prev_sign) crossings++;
        prev_sign = sign;
    }

    /* Each full ripple cycle produces two zero crossings. */
    if(crossings >= 2 && pkpk >= 10 && sample_rate_hz > 0.0f) {
        float duration_s = (float)n / sample_rate_hz;
        out->flicker_hz = ((float)crossings / 2.0f) / duration_s;
        out->flicker_valid = true;
    } else {
        out->flicker_hz = 0.0f;
        out->flicker_valid = false;
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    Reading* r = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Light + Flicker");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);
    char line[32];

    snprintf(line, sizeof(line), "Light:   %4.0f mV", (double)r->light_mv);
    canvas_draw_str(canvas, 0, 23, line);

    snprintf(line, sizeof(line), "Level:   %4.0f %%", (double)r->light_percent);
    canvas_draw_str(canvas, 0, 33, line);

    snprintf(line, sizeof(line), "Flicker: %4.1f %%", (double)r->flicker_percent);
    canvas_draw_str(canvas, 0, 43, line);

    if(r->flicker_valid) {
        if(r->flicker_hz < 1000.0f) {
            snprintf(line, sizeof(line), "Freq:    %4.0f Hz", (double)r->flicker_hz);
        } else {
            snprintf(line, sizeof(line), "Freq:    %4.1f kHz", (double)(r->flicker_hz / 1000.0f));
        }
    } else {
        snprintf(line, sizeof(line), "Freq:     ---");
    }
    canvas_draw_str(canvas, 0, 53, line);

    snprintf(line, sizeof(line), "Temp:    %4.1f C", (double)r->temp_c);
    canvas_draw_str(canvas, 0, 63, line);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}

int32_t kiisu_light_adc_main(void* p) {
    UNUSED(p);

    Reading reading = {0};
    uint16_t* burst = malloc(BURST_SAMPLES * sizeof(uint16_t));

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &reading);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullDown, GpioSpeedHigh);

    /* Enable DWT cycle counter (idempotent — safe even if already on) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    FuriHalAdcHandle* adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(adc);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            if(event.type == InputTypePress && event.key == InputKeyBack) {
                running = false;
            }
            continue;
        }

        float sample_rate = 0.0f;
        burst_sample(adc, burst, BURST_SAMPLES, &sample_rate);

        analyze(burst, BURST_SAMPLES, sample_rate, &reading);

        /* DC level from the burst mean — more stable than a single sample */
        uint64_t sum = 0;
        for(size_t i = 0; i < BURST_SAMPLES; i++) sum += burst[i];
        uint16_t mean_raw = (uint16_t)(sum / BURST_SAMPLES);
        reading.light_mv = furi_hal_adc_convert_to_voltage(adc, mean_raw);
        float pct = 100.0f - (reading.light_mv / LIGHT_FULL_SCALE * 100.0f);
        if(pct < 0.0f) pct = 0.0f;
        if(pct > 100.0f) pct = 100.0f;
        reading.light_percent = pct;

        reading.temp_c = furi_hal_adc_convert_temp(
            adc, furi_hal_adc_read(adc, FuriHalAdcChannelTEMPSENSOR));

        view_port_update(view_port);
    }

    furi_hal_adc_release(adc);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(burst);

    return 0;
}
