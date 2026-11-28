#pragma once

#include "../LuaScriptManager.h"
#include "../Binder.h"

namespace Atlas::Scripting::Bindings {

    void GenerateSceneBindings(Binder* ns);

    void GenerateEntityBindings(Binder* ns);

    void GenerateComponentBindings(Binder* ns);

}