#include "LuaScriptComponentPanel.h"
#include "resource/ResourceManager.h"

#include "imgui_stdlib.h"

namespace Atlas::Editor::UI
{
    bool LuaScriptComponentPanel::Render(Scene::Entity entity, LuaScriptComponent &luaScriptComponent)
    {
        auto buttonName = luaScriptComponent.script.IsValid() ? luaScriptComponent.script.GetResource()->GetFileName() :
            "Drop script resource here";
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        if (ImGui::BeginDragDropTarget()) {
            if (auto dropPayload = ImGui::AcceptDragDropPayload(typeid(Scripting::Script).name())) {
                Resource<Scripting::Script>* resource;
                std::memcpy(&resource, dropPayload->Data, dropPayload->DataSize);
                // We know this mesh is loaded, so we can just request a handle without loading
                luaScriptComponent.script = ResourceManager<Scripting::Script>::GetResource(resource->path);
                // Usually we need to also keep track of resource in the scene, ignore for now
                // resourceChanged = true;
            }

            ImGui::EndDragDropTarget();
        }

        if (!luaScriptComponent.script.IsLoaded())
            return false;

        ImGui::InputTextMultiline("Code", &luaScriptComponent.script->code);

        return false;
    }
}