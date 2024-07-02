#include "lora_tester_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const lora_tester_scene_on_enter_handlers[])(void*) = {
#include "lora_tester_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const lora_tester_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "lora_tester_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const lora_tester_scene_on_exit_handlers[])(void* context) = {
#include "lora_tester_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers lora_tester_scene_handlers = {
    .on_enter_handlers = lora_tester_scene_on_enter_handlers,
    .on_event_handlers = lora_tester_scene_on_event_handlers,
    .on_exit_handlers = lora_tester_scene_on_exit_handlers,
    .scene_num = LoraTesterSceneNum,
};