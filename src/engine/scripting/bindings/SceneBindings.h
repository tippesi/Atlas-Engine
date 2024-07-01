#pragma once

#include "../LuaScriptManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateSceneBindings(sol::table* ns);

    void GenerateEntityBindings(sol::table* ns);

    void GenerateComponentBindings(sol::table* ns);

}