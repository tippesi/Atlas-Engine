#include "LuaScriptComponentPanel.h"

#include "imgui_stdlib.h"

namespace Atlas::Editor::UI
{
    bool LuaScriptComponentPanel::Render(Scene::Entity entity, LuaScriptComponent &luaScriptComponent)
    {

        ImGui::InputTextMultiline("Code", &luaScriptComponent.code);

        return false;
    }
}