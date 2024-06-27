#pragma once

#include "../LuaScriptManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateUtilityBindings(sol::table* ns);

    void GenerateVolumeBindings(sol::table* ns);

}