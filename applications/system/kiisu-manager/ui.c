#include "ui.h"
#include <furi.h>

struct KiisuUi {
    ViewDispatcher* vd;
    VariableItemList* list;
    Popup* popup;
};

KiisuUi* kiisu_ui_alloc(ViewDispatcher* vd) {
    KiisuUi* ui = malloc(sizeof(KiisuUi));
    ui->vd = vd;
    ui->list = variable_item_list_alloc();
    ui->popup = popup_alloc();

    view_dispatcher_add_view(vd, 0, variable_item_list_get_view(ui->list));
    view_dispatcher_add_view(vd, 1, popup_get_view(ui->popup));
    return ui;
}

void kiisu_ui_free(KiisuUi* ui) {
    if(!ui) return;
    if(ui->vd) {
        view_dispatcher_remove_view(ui->vd, 1);
        view_dispatcher_remove_view(ui->vd, 0);
    }
    popup_free(ui->popup);
    variable_item_list_free(ui->list);
    ui->popup = NULL;
    ui->list = NULL;
    ui->vd = NULL;
    free(ui);
}

VariableItemList* kiisu_ui_get_list(KiisuUi* ui) { return ui->list; }
Popup* kiisu_ui_get_popup(KiisuUi* ui) { return ui->popup; }

void kiisu_ui_show_list(KiisuUi* ui) { view_dispatcher_switch_to_view(ui->vd, 0); }

void kiisu_ui_show_popup(KiisuUi* ui, const char* title, const char* text, uint32_t ms, KiisuUiOnExit cb, void* ctx) {
    popup_reset(ui->popup);
    if(title) popup_set_header(ui->popup, title, 64, 10, AlignCenter, AlignTop);
    if(text) popup_set_text(ui->popup, text, 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(ui->popup, ms);
    popup_set_context(ui->popup, ctx);
    popup_set_callback(ui->popup, cb);
    popup_enable_timeout(ui->popup);
    view_dispatcher_switch_to_view(ui->vd, 1);
}
