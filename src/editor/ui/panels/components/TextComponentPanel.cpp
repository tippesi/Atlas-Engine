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

        ImGui::InputTextMultiline("##Text", &textComponent.text,
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f + ImGui::GetStyle().FramePadding.y * 2.0f));

        ImGui::SeparatorText("Layout");

        Widgets::DragVec3("Position", textComponent.position, 0.1f, 0.0f);
        ImGui::DragFloat2("Half size", glm::value_ptr(textComponent.halfSize), 0.1f);

        ImGui::SeparatorText("Style");

        ImGui::DragFloat("Scale", &textComponent.textScale, 0.01f, 0.0f, 100.0f);
        ImGui::ColorEdit4("Color", glm::value_ptr(textComponent.textColor));
        ImGui::ColorEdit4("Outline color", glm::value_ptr(textComponent.outlineColor));
        ImGui::DragFloat("Outline factor", &textComponent.outlineFactor, 0.01f, 0.0f, 1.0f);

        ImGui::PopID();

        return false;

    }

}