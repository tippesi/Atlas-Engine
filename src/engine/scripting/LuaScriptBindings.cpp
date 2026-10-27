#include "LuaScriptBindings.h"
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
#include "bindings/MathBindings.h"
#include "bindings/MeshBindings.h"
#include "bindings/AnimationBindings.h"
#include "bindings/PhysicsBindings.h"
#include "bindings/PostProcessingBindings.h"
#include "bindings/ResourceBindings.h"
#include "bindings/SceneBindings.h"
#include "bindings/UtilityBindings.h"

namespace Atlas::Scripting {

    LuaScriptBindings::LuaScriptBindings(Ref<sol::state> luaState, sol::table* atlasNs, sol::table* glmNs) {

        this->luaState = luaState;
        this->atlasNs = atlasNs;
        this->glmNs = glmNs;

    }

    void LuaScriptBindings::GenerateBindings() {

        // Math types need to be registered before the bindings that use them as members
        Bindings::GenerateMathBindings(glmNs);
        Bindings::GenerateSceneBindings(atlasNs);
        Bindings::GenerateEntityBindings(atlasNs);
        Bindings::GenerateComponentBindings(atlasNs);
        Bindings::GenerateUtilityBindings(atlasNs);
        Bindings::GenerateMaterialBindings(atlasNs);
        Bindings::GenerateAudioBindings(atlasNs);
        Bindings::GenerateMeshBindings(atlasNs);
        Bindings::GenerateAnimationBindings(atlasNs);
        Bindings::GeneratePhysicsBindings(atlasNs);
        Bindings::GenerateVolumeBindings(atlasNs);
        Bindings::GenerateLightingBindings(atlasNs);
        Bindings::GeneratePostProcessingBindings(atlasNs);
        Bindings::GenerateKeyboardBindings(atlasNs);
        Bindings::GenerateControllerBindings(atlasNs);
        Bindings::GenerateGraphicBindings(atlasNs);
        Bindings::GenerateResourceManagerBindings(atlasNs);

    }    

}
