#include "LuaScriptBindings.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "Clock.h"
#include "input/KeyboardMap.h"
#include "loader/ModelLoader.h"

#include "bindings/AudioBindings.h"
#include "bindings/GraphicsBindings.h"
#include "bindings/InputBindings.h"
#include "bindings/LightingBindings.h"
#include "bindings/MathBindings.h"
#include "bindings/MeshBindings.h"
#include "bindings/PhysicsBindings.h"
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

        Bindings::GenerateSceneBindings(atlasNs);
        Bindings::GenerateEntityBindings(atlasNs);
        Bindings::GenerateComponentBindings(atlasNs);
        Bindings::GenerateUtilityBindings(atlasNs);
        Bindings::GenerateMaterialBindings(atlasNs);
        Bindings::GenerateAudioBindings(atlasNs);
        Bindings::GenerateMeshBindings(atlasNs);
        Bindings::GeneratePhysicsBindings(atlasNs);
        Bindings::GenerateMathBindings(glmNs);
        Bindings::GenerateVolumeBindings(atlasNs);
        Bindings::GenerateLightingBindings(atlasNs);
        Bindings::GenerateKeyboardBindings(atlasNs);
        Bindings::GenerateGraphicBindings(atlasNs);
        Bindings::GenerateResourceManagerBindings(atlasNs);

    }    

}