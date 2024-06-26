#include "LuaScriptComponentPanel.h"

#include "../../../tools/ResourcePayloadHelper.h"

#include "imgui_stdlib.h"

namespace Atlas::Editor::UI
{
    bool LuaScriptComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, LuaScriptComponent &luaScriptComponent)
    {
        auto buttonName = luaScriptComponent.script.IsValid() ? luaScriptComponent.script.GetResource()->GetFileName() : "Drop script resource here";
        if (ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0}))
            scriptSelectionPopup.Open();

        // Such that drag and drop will work from the content browser
        if (ImGui::IsDragDropActive() && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
            ImGui::SetWindowFocus();
            ImGui::SetItemDefaultFocus();
        }

        auto handle = ResourcePayloadHelper::AcceptDropResource<Scripting::Script>();
        if (handle.IsValid()) {
            luaScriptComponent.ChangeResource(handle);
        }

        auto scriptResources = ResourceManager<Scripting::Script>::GetResources();
        handle = scriptSelectionPopup.Render(scriptResources);

        if (handle.IsValid()) {
            luaScriptComponent.ChangeResource(handle);
        }

        if (!luaScriptComponent.script.IsLoaded())
            return false;

        ImGui::Checkbox("Permanent execution", &luaScriptComponent.permanentExecution);
        ImGui::InputTextMultiline("Code", &luaScriptComponent.script->code, ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);

        ImGui::Separator();
        ImGui::Text("Script defined properties:");
        for (auto &property : luaScriptComponent.properties)
        {
            switch (property.type)
            {
            case LuaScriptComponent::PropertyType::Boolean:
                ImGui::Checkbox(property.name.c_str(), &property.booleanValue);
                break;
            case LuaScriptComponent::PropertyType::Integer:
                ImGui::InputInt(property.name.c_str(), &property.integerValue);
                break;
            case LuaScriptComponent::PropertyType::Double:
                ImGui::InputDouble(property.name.c_str(), &property.doubleValue);
                break;
            case LuaScriptComponent::PropertyType::String:
                ImGui::InputText(property.name.c_str(), &property.stringValue);
                break;
            }
        }

        return false;
    }
}