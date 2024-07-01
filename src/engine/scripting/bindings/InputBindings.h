#pragma once

#include "../LuaScriptManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateKeyboardBindings(sol::table* ns);

}