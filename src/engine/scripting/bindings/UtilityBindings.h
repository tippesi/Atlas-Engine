#pragma once

#include "../LuaScriptManager.h"
#include "../Binder.h"

namespace Atlas::Scripting::Bindings {

    void GenerateUtilityBindings(Binder* ns);

    void GenerateVolumeBindings(Binder* ns);

}