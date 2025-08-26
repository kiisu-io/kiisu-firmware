#pragma once

#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>

typedef struct KiisuUi KiisuUi;

typedef void (*KiisuUiOnExit)(void* ctx);

KiisuUi* kiisu_ui_alloc(ViewDispatcher* vd);
void kiisu_ui_free(KiisuUi* ui);

VariableItemList* kiisu_ui_get_list(KiisuUi* ui);
Popup* kiisu_ui_get_popup(KiisuUi* ui);

void kiisu_ui_show_list(KiisuUi* ui);
void kiisu_ui_show_popup(KiisuUi* ui, const char* title, const char* text, uint32_t ms, KiisuUiOnExit cb, void* ctx);
