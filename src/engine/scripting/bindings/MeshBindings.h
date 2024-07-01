#pragma once

#include "../LuaScriptManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateMeshBindings(sol::table* ns);

    void GenerateMaterialBindings(sol::table* ns);

}