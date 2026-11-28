#include "LuaScriptBindings.h"
#include "project/Project.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "Clock.h"
#include "input/KeyboardMap.h"
#include "loader/ModelImporter.h"

#include "bindings/AudioBindings.h"
#include "bindings/GraphicsBindings.h"
#include "bindings/InputBindings.h"
#include "bindings/LightingBindings.h"
#include "bindings/ProjectBindings.h"
#include "bindings/MaterialGraphBindings.h"
#include "bindings/MathBindings.h"
#include "bindings/MeshBindings.h"
#include "bindings/NavigationBindings.h"
#include "bindings/AnimationBindings.h"
#include "bindings/PhysicsBindings.h"
#include "bindings/PostProcessingBindings.h"
#include "bindings/ResourceBindings.h"
#include "bindings/SceneBindings.h"
#include "bindings/StreamingBindings.h"
#include "bindings/UtilityBindings.h"

namespace Atlas::Scripting {

    LuaScriptBindings::LuaScriptBindings(Ref<sol::state> luaState, sol::table* atlasNs, sol::table* glmNs) {

        this->luaState = luaState;
        this->atlasNs = atlasNs;
        this->glmNs = glmNs;

    }

    void LuaScriptBindings::GenerateBindings() {

        Binder atlas(*atlasNs, "Atlas");
        Binder glm(*glmNs, "Glm");

        // Math types need to be registered before the bindings that use them as members
        Bindings::GenerateMathBindings(&glm);
        Bindings::GenerateSceneBindings(&atlas);
        Bindings::GenerateEntityBindings(&atlas);
        Bindings::GenerateComponentBindings(&atlas);
        Bindings::GenerateUtilityBindings(&atlas);
        Bindings::GenerateMaterialGraphBindings(&atlas);
        Bindings::GenerateMaterialBindings(&atlas);
        Bindings::GenerateAudioBindings(&atlas);
        Bindings::GenerateMeshBindings(&atlas);
        Bindings::GenerateAnimationBindings(&atlas);
        Bindings::GeneratePhysicsBindings(&atlas);
        Bindings::GenerateNavigationBindings(&atlas);
        Bindings::GenerateVolumeBindings(&atlas);
        Bindings::GenerateLightingBindings(&atlas);
        Bindings::GeneratePostProcessingBindings(&atlas);
        Bindings::GenerateKeyboardBindings(&atlas);
        Bindings::GenerateControllerBindings(&atlas);
        Bindings::GenerateGraphicBindings(&atlas);
        Bindings::GenerateResourceManagerBindings(&atlas);
        Bindings::GenerateStreamingBindings(&atlas);
        Bindings::GenerateProjectBindings(&atlas);

        // Scripts get these in their environment (LuaScriptComponent), they are recorded before any script runs
        Binder environment(luaState->create_table(), "_G");
        environment.set_function("GetThisEntity", []() { return Scene::Entity(); });
        environment.set_function("GetThisScene", []() { return static_cast<Scene::Scene*>(nullptr); });

        // Every script works with the activated project, the engine also runs without one
        Binder globals(luaState->globals(), "_G");
        if (Project::active)
            globals.set("Project", Project::active);

    }    

}
