#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <notification/notification.h>
#include <notification/notification_app.h>
#include <toolbox/saved_struct.h>
#include <notification/notification_messages.h>

#include "settings_client.h"
#include "ui.h"

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    KiisuUi* ui;
    // Local-only
    uint32_t sleep_delay_ms;
    VariableItem* it_sleep;
    NotificationApp* notification;
    VariableItem* it_brightness;
    VariableItem* it_poweroff;
    VariableItem* it_startup;
    VariableItem* it_rainbow;
} App;

static const char* poweroff_opts[] = {
    "Off", "15s", "30s", "1m", "5m", "10m", "30m", "60m",
};

static const char* startup_color_opts[] = {
    "Purple", "Off", "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White",
};

// Same logic/options as Notification Settings Backlight Time
#define DELAY_COUNT 12
static const char* const delay_text[DELAY_COUNT] = {
    "Always ON",
    "1s",
    "5s",
    "10s",
    "15s",
    "30s",
    "60s",
    "90s",
    "120s",
    "5min",
    "10min",
    "30min",
};
static const uint32_t delay_value[DELAY_COUNT] = {
    0, 1000, 5000, 10000, 15000, 30000, 60000, 90000, 120000, 300000, 600000, 1800000,
};

static void set_item_value_text_u8(VariableItem* it, uint8_t val, const char* suffix) {
    char buf[16];
    if(suffix)
        snprintf(buf, sizeof(buf), "%u%s", val, suffix);
    else
        snprintf(buf, sizeof(buf), "%u", val);
    variable_item_set_current_value_text(it, buf);
}

// UI text setters without side effects
static void brightness_set_text_from_index(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    uint8_t value = (idx == 0) ? 0 : (uint8_t)(idx * 5);
    set_item_value_text_u8(it, value, value ? "%" : " (Auto)");
}

static void poweroff_set_text_from_index(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    if(idx > 7) idx = 7;
    variable_item_set_current_value_text(it, poweroff_opts[idx]);
}

static void startup_set_text_from_index(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    if(idx > 8) idx = 8;
    variable_item_set_current_value_text(it, startup_color_opts[idx]);
}

static void rainbow_set_text_from_index(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, idx ? "On" : "Off");
}

static void sleep_set_text_from_index(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    if(idx >= DELAY_COUNT) idx = 0;
    variable_item_set_current_value_text(it, delay_text[idx]);
}

static void brightness_changed(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it);
    // index 0 = Auto(0), else 5..100 step 5
    uint8_t value = (idx == 0) ? 0 : (uint8_t)(idx * 5);
    set_item_value_text_u8(it, value, value ? "%" : " (Auto)");
    kiisu_settings_write_u8(REG_LED_BRIGHTNESS, value);
}

static void poweroff_changed(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it); // 0..7
    variable_item_set_current_value_text(it, poweroff_opts[idx]);
    kiisu_settings_write_u8(REG_AUTO_POWEROFF, idx);
}

static void startup_changed(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it); // 0..8
    variable_item_set_current_value_text(it, startup_color_opts[idx]);
    kiisu_settings_write_u8(REG_STARTUP_COLOR, idx);
}

static void rainbow_changed(VariableItem* it) {
    uint8_t idx = variable_item_get_current_value_index(it); // 0=Off,1=On
    variable_item_set_current_value_text(it, idx ? "On" : "Off");
    kiisu_settings_write_u8(REG_CHARGE_RAINBOW, idx);
}

static void sleep_changed(VariableItem* it) {
    App* app = variable_item_get_context(it);
    if(!app) return;
    uint8_t idx = variable_item_get_current_value_index(it);
    if(idx >= DELAY_COUNT) idx = 0;
    app->sleep_delay_ms = delay_value[idx];
    variable_item_set_current_value_text(it, delay_text[idx]);
    // Apply to system notification settings immediately
    if(app->notification) {
        app->notification->settings.display_off_delay_ms = app->sleep_delay_ms;
        // Wake display so the new timer restarts from now
        notification_message(app->notification, &sequence_display_backlight_on);
        // Optional: notify internal layer (exported) that settings changed
        notification_internal_message(app->notification, &sequence_display_backlight_on);
        // Persist settings so they survive app switches and restarts
        saved_struct_save(
            NOTIFICATION_SETTINGS_PATH,
            &app->notification->settings,
            sizeof(app->notification->settings),
            NOTIFICATION_SETTINGS_MAGIC,
            NOTIFICATION_SETTINGS_VERSION);
    }
}

static void popup_done(void* ctx) {
    App* app = ctx;
    if(app && app->vd) {
        kiisu_ui_show_list(app->ui);
    }
}

static void build_menu(App* app) {
    VariableItemList* list = kiisu_ui_get_list(app->ui);
    variable_item_list_reset(list);

    // 1) Time until sleep (local only)
    app->it_sleep = variable_item_list_add(list, "Time until sleep", DELAY_COUNT, sleep_changed, app);
    // Load from current local setting, default Always ON
    uint8_t idx = 0;
    for(uint8_t i = 0; i < DELAY_COUNT; i++) {
        if(delay_value[i] == app->sleep_delay_ms) { idx = i; break; }
    }
    variable_item_set_current_value_index(app->it_sleep, idx);
    sleep_set_text_from_index(app->it_sleep);

    // Brightness: Auto + 5..100 step 5 => 1 + 20 = 21 steps, index 0..20
    app->it_brightness = variable_item_list_add(list, "LED Brightness", 21, brightness_changed, NULL);
    // attempt to read; fallback Auto
    uint8_t v = 0; idx = 0;
    if(kiisu_settings_read_u8(REG_LED_BRIGHTNESS, &v)) {
        if(v == 0) idx = 0; else { idx = v / 5; if(idx > 20) idx = 20; }
    }
    variable_item_set_current_value_index(app->it_brightness, idx);
    brightness_set_text_from_index(app->it_brightness);

    // Auto Power-Off 0..7
    app->it_poweroff = variable_item_list_add(list, "Auto Power-Off", 8, poweroff_changed, NULL);
    v = 1; idx = 1; // default 15s
    if(kiisu_settings_read_u8(REG_AUTO_POWEROFF, &v)) {
        idx = (v <= 7) ? v : 1;
    }
    variable_item_set_current_value_index(app->it_poweroff, idx);
    poweroff_set_text_from_index(app->it_poweroff);

    // Startup color 0..8
    app->it_startup = variable_item_list_add(list, "Startup Color", 9, startup_changed, NULL);
    v = 0; idx = 0;
    if(kiisu_settings_read_u8(REG_STARTUP_COLOR, &v)) {
        idx = (v <= 8) ? v : 0;
    }
    variable_item_set_current_value_index(app->it_startup, idx);
    startup_set_text_from_index(app->it_startup);

    // Charge rainbow 0/1
    app->it_rainbow = variable_item_list_add(list, "Charge Rainbow", 2, rainbow_changed, NULL);
    v = 1; idx = 1;
    if(kiisu_settings_read_u8(REG_CHARGE_RAINBOW, &v)) {
        idx = (v != 0) ? 1 : 0;
    }
    variable_item_set_current_value_index(app->it_rainbow, idx);
    rainbow_set_text_from_index(app->it_rainbow);

    variable_item_list_set_selected_item(list, 0);
}

// Back button exits the app
static bool on_nav_back(void* ctx) {
    App* app = ctx;
    view_dispatcher_stop(app->vd);
    return true;
}

static void app_free(App* app) {
    if(!app) return;
    kiisu_ui_free(app->ui);
    view_dispatcher_free(app->vd);
    if(app->notification) {
        furi_record_close(RECORD_NOTIFICATION);
    }
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t kiisu_manager_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->sleep_delay_ms = app->notification ? app->notification->settings.display_off_delay_ms : 0; // default Always ON if missing
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, on_nav_back);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    app->ui = kiisu_ui_alloc(app->vd);

    if(!kiisu_settings_probe_ready()) {
        kiisu_ui_show_popup(app->ui, "Kiisu Manager", "Device @0x31 not found", 1500, popup_done, app);
        // still show menu after popup to allow retry writes
    }

    build_menu(app);
    kiisu_ui_show_list(app->ui);

    view_dispatcher_run(app->vd);
    app_free(app);
    return 0;
}
