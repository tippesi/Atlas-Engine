#pragma once

#include "../LuaScriptManager.h"
#include "../Binder.h"

namespace Atlas::Scripting::Bindings {

    void GenerateMeshBindings(Binder* ns);

    void GenerateMaterialBindings(Binder* ns);

}