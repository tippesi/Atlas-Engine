#include "TextComponentPanel.h"
#include "../../Widgets.h"

#include <imgui_stdlib.h>

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool TextComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, 
        TextComponent& textComponent) {

        ImGui::PushID(GetNameID());

        bool hasFont = textComponent.font.IsValid();
        auto buttonName = hasFont ? textComponent.font.GetResource()->GetFileName() : "Drop font resource here";
        Widgets::DropSlot(buttonName.c_str(), !hasFont);

        auto handle = ResourcePayloadHelper::AcceptDropResource<Font>();
        if (handle.IsValid()) {
            textComponent.ChangeResource(handle);
        }

        Widgets::TextArea("Text", textComponent.text, 7.0f);

        ImGui::SeparatorText("Layout");

        if (Widgets::BeginProperties("TextLayout")) {
            Widgets::PropertyLabel("Position");
            Widgets::DragVec3("##Position", textComponent.position, 0.1f, 0.0f);
            Widgets::PropertyLabel("Half size");
            ImGui::DragFloat2("##Half size", glm::value_ptr(textComponent.halfSize), 0.1f);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Style");

        if (Widgets::BeginProperties("TextStyle")) {
            Widgets::PropertyLabel("Scale");
            ImGui::DragFloat("##Scale", &textComponent.textScale, 0.01f, 0.0f, 100.0f);
            Widgets::PropertyLabel("Color");
            ImGui::ColorEdit4("##Color", glm::value_ptr(textComponent.textColor));
            Widgets::PropertyLabel("Outline color");
            ImGui::ColorEdit4("##Outline color", glm::value_ptr(textComponent.outlineColor));
            Widgets::PropertyLabel("Outline factor");
            ImGui::DragFloat("##Outline factor", &textComponent.outlineFactor, 0.01f, 0.0f, 1.0f);
            Widgets::EndProperties();
        }

        ImGui::PopID();

        return false;

    }

}