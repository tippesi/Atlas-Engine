#include "TextComponentPanel.h"

#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool TextComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, TextComponent& textComponent) {

        ImGui::PushID(GetNameID());

        auto buttonName = textComponent.font.IsValid() ? textComponent.font.GetResource()->GetFileName() :
            "Drop font resource here";
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        if (ImGui::BeginDragDropTarget()) {
            if (auto dropPayload = ImGui::AcceptDragDropPayload(typeid(Font).name())) {
                Resource<Font>* resource;
                std::memcpy(&resource, dropPayload->Data, dropPayload->DataSize);

                auto font = ResourceManager<Font>::GetResource(resource->path);
                textComponent.ChangeResource(font);
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::InputTextMultiline("Text", &textComponent.text);

        ImGui::Text("General properties");

        ImGui::DragFloat3("Position", glm::value_ptr(textComponent.position), 0.1f);
        ImGui::DragFloat2("Half size", glm::value_ptr(textComponent.halfSize), 0.1f);

        ImGui::Text("Text properties");

        ImGui::DragFloat("Scale", &textComponent.textScale, 0.01f, 0.0f, 100.0f);
        ImGui::ColorEdit4("Color", glm::value_ptr(textComponent.textColor));
        ImGui::ColorEdit4("Outline color", glm::value_ptr(textComponent.outlineColor));
        ImGui::DragFloat("Outline factor", &textComponent.outlineFactor, 0.01f, 0.0f, 1.0f);

        ImGui::PopID();

        return false;

    }

}