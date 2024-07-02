#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) LoraTesterScene##id,
typedef enum {
#include "lora_tester_scene_config.h"
    LoraTesterSceneNum,
} LoraTesterScene;
#undef ADD_SCENE

extern const SceneManagerHandlers lora_tester_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "lora_tester_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "lora_tester_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "lora_tester_scene_config.h"
#undef ADD_SCENE