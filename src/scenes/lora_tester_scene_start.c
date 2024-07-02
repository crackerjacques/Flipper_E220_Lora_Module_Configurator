#include "../lora_tester_app_i.h"

typedef enum {
    LoraTesterItemLoRaMode,
    LoraTesterItemBaudRate,
    LoraTesterItemConfigure,
    LoraTesterItemEnterAddress,
    LoraTesterItemConfigEncryptionKey,
    LoraTesterItemExportConfig,
    LoraTesterItemLoadConfig,
    LoraTesterItemReceive,
    LoraTesterItemStats,
    LoraTesterItemAbout,
    LoraTesterItemCount
} LoraTesterItem;

static void lora_tester_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    LoraTesterApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void lora_tester_scene_start_var_list_change_callback(VariableItem* item) {
    LoraTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, lora_mode_names[index]);
    lora_tester_set_mode(app, (LoRaMode)index);
}

static void lora_tester_scene_start_baud_rate_change_callback(VariableItem* item) {
    LoraTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->baud_rate = baud_rates[index];
    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "%lu bps", app->baud_rate);
    variable_item_set_current_value_text(item, baud_str);
}

void lora_tester_scene_start_on_enter(void* context) {
    LoraTesterApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_reset(var_item_list);

    VariableItem* item = variable_item_list_add(
        var_item_list, "LoRa Mode", 4, lora_tester_scene_start_var_list_change_callback, app);
    variable_item_set_current_value_index(item, (uint8_t)app->current_mode);
    variable_item_set_current_value_text(item, lora_mode_names[app->current_mode]);

    VariableItem* baud_item = variable_item_list_add(
        var_item_list,
        "Baud Rate",
        baud_rate_count,
        lora_tester_scene_start_baud_rate_change_callback,
        app);

    uint8_t current_baud_index = 0;
    for(uint8_t i = 0; i < baud_rate_count; i++) {
        if(baud_rates[i] == app->baud_rate) {
            current_baud_index = i;
            break;
        }
    }

    variable_item_set_current_value_index(baud_item, current_baud_index);
    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "%lu bps", app->baud_rate);
    variable_item_set_current_value_text(baud_item, baud_str);

    const char* menu_items[] = {
        "Configure",
        "Enter Address",
        "Config Encryption Key",
        "Export Config",
        "Load Config",
        "Receive",
        "Stats",
        "About"};
    for(unsigned int i = 0; i < COUNT_OF(menu_items); i++) {
        variable_item_list_add(var_item_list, menu_items[i], 0, NULL, NULL);
    }

    variable_item_list_set_enter_callback(
        var_item_list, lora_tester_scene_start_var_list_enter_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, LoraTesterSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewVarItemList);
}

bool lora_tester_scene_start_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case LoraTesterItemConfigure:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneConfigure);
            consumed = true;
            break;
        case LoraTesterItemEnterAddress:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneAddressInput);
            consumed = true;
            break;
        case LoraTesterItemConfigEncryptionKey:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneEncryptionKey);
            consumed = true;
            break;
        case LoraTesterItemExportConfig:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneExportConfig);
            consumed = true;
            break;
        case LoraTesterItemLoadConfig:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneConfigLoad);
            consumed = true;
            break;
        case LoraTesterItemReceive:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneReceive);
            consumed = true;
            break;
        case LoraTesterItemStats:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneStat);
            consumed = true;
            break;
        case LoraTesterItemAbout:
            scene_manager_next_scene(app->scene_manager, LoraTesterSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void lora_tester_scene_start_on_exit(void* context) {
    LoraTesterApp* app = context;
    variable_item_list_reset(app->var_item_list);
}