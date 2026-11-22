#pragma once

#include "../LuaScriptManager.h"
#include "../Binder.h"

namespace Atlas::Scripting::Bindings {

    void GenerateKeyboardBindings(Binder* ns);

    void GenerateControllerBindings(Binder* ns);

}