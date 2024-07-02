#include "../lora_tester_app_i.h"
#include "lora_tester_icons.h"

static void
    lora_tester_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    LoraTesterApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void lora_tester_scene_about_on_enter(void* context) {
    LoraTesterApp* app = context;
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 0, 0, &I_about95);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", lora_tester_scene_about_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewWidget);
}

bool lora_tester_scene_about_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(app->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void lora_tester_scene_about_on_exit(void* context) {
    LoraTesterApp* app = context;

    widget_reset(app->widget);
}