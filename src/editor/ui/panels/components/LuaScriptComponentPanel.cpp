#include "LuaScriptComponentPanel.h"
#include "../../Widgets.h"

#include "../../../tools/ResourcePayloadHelper.h"

#include "imgui_stdlib.h"

namespace Atlas::Editor::UI
{
    bool LuaScriptComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, LuaScriptComponent &luaScriptComponent)
    {
        bool hasScript = luaScriptComponent.script.IsValid();
        auto buttonName = hasScript ? luaScriptComponent.script.GetResource()->GetFileName() : "Drop script resource here";
        if (Widgets::DropSlot(buttonName.c_str(), !hasScript))
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
        // The code is read-only here and can be long, so it is collapsed by default
        if (ImGui::TreeNodeEx("Source code", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGui::InputTextMultiline("##Code", &luaScriptComponent.script->code,
                ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 12.0f), ImGuiInputTextFlags_ReadOnly);
            ImGui::TreePop();
        }

        ImGui::SeparatorText("Properties");
        if (luaScriptComponent.properties.empty())
            ImGui::TextDisabled("This script doesn't define any properties");
        for (auto& [name, property] : luaScriptComponent.properties) {
            switch (property.type) {
            case LuaScriptComponent::PropertyType::Boolean:
                ImGui::Checkbox(name.c_str(), &property.booleanValue);
                break;
            case LuaScriptComponent::PropertyType::Integer:
                ImGui::InputInt(name.c_str(), &property.integerValue);
                break;
            case LuaScriptComponent::PropertyType::Double:
                ImGui::InputDouble(name.c_str(), &property.doubleValue);
                break;
            case LuaScriptComponent::PropertyType::String:
                ImGui::InputText(name.c_str(), &property.stringValue);
                break;
            case LuaScriptComponent::PropertyType::Vec2:
                ImGui::DragFloat2(name.c_str(), glm::value_ptr(property.vec2Value));
                break;
            case LuaScriptComponent::PropertyType::Vec3:
                Widgets::DragVec3(name.c_str(), property.vec3Value, 1.0f, 0.0f);
                break;
            case LuaScriptComponent::PropertyType::Undefined:
            default:
                break;
            }
        }

        return false;
    }
}