#include "SplineComponentPanel.h"
#include <Singletons.h>

#include <imgui_stdlib.h>

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool SplineComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, 
        SplineComponent& splineComponent) {

        ImGui::PushID(GetNameID());

        const char* typeItems[] = { "Linear", "Catmull Rom", "Hermite" };
        auto type = static_cast<int32_t>(splineComponent.type);
        auto prevItem = type;
        ImGui::Combo("Type", &type, typeItems, IM_ARRAYSIZE(typeItems));
        splineComponent.type = static_cast<SplineComponent::SplineType>(type);

        ImGui::Separator();
        ImGui::Text("Points");

        auto& controlPoints = splineComponent.controlPoints;

        size_t counter = 0;
        size_t modifyIdx = 0;
        ControlPointAction action = ControlPointAction::None;
        for (auto& controlPoint : controlPoints) {
            ControlPointAction currentAction = ControlPointAction::None;
            RenderControlPoint(controlPoint, counter++, currentAction);

            if (currentAction != ControlPointAction::None) {
                action = currentAction;
                modifyIdx = counter - 1;
            }
        }

        switch(action) {
            case ControlPointAction::Delete: 
                controlPoints.erase(controlPoints.begin() + modifyIdx);
                break;
            case ControlPointAction::MoveUp:
                std::swap(controlPoints[std::max(size_t(0), modifyIdx - 1)], controlPoints[modifyIdx]);
                break;
            case ControlPointAction::MoveDown:
                std::swap(controlPoints[std::min(controlPoints.size() - 1, modifyIdx + 1)], controlPoints[modifyIdx]);
                break;
            default: break;
        }

        auto region = ImGui::GetContentRegionAvail();
        if (ImGui::Button("Add point", ImVec2(region.x, 0.0f))) {
            auto controlPoint = controlPoints.empty() ? SplineControlPoint{} : controlPoints.back();
            controlPoints.push_back(controlPoint);
        }

        ImGui::PopID();

        return false;

    }

    void SplineComponentPanel::RenderControlPoint(SplineControlPoint& point, size_t idx, ControlPointAction& action) {

        const float padding = 8.0f;
        const ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
            | ImGuiTreeNodeFlags_AllowOverlap;

        auto region = ImGui::GetContentRegionAvail();
        auto lineHeight = ImGui::GetTextLineHeight();
        auto deleteButtonSize = ImVec2(lineHeight, lineHeight);        

        auto treeNodeSize = region.x - deleteButtonSize.x + 2.0f * padding;
        ImGui::SetNextItemWidth(treeNodeSize);

        bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(idx), nodeFlags, "Control point %d", int32_t(idx));
        ImGui::SameLine();
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        auto upArrowIcon = Singletons::icons->Get(IconType::ArrowUp);
        auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&upArrowIcon);
        if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
            action = ControlPointAction::MoveUp;
        }
        ImGui::SameLine();

        auto downArrowIcon = Singletons::icons->Get(IconType::ArrowDown);
        set = Singletons::imguiWrapper->GetTextureDescriptorSet(&downArrowIcon);
        if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
            action = ControlPointAction::MoveDown;
        }
        ImGui::SameLine();

        auto& deleteIcon = Singletons::icons->Get(IconType::Trash);
        set = Singletons::imguiWrapper->GetTextureDescriptorSet(&deleteIcon);
        if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
            action = ControlPointAction::Delete;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (!open)
            return;

         // The matrix decomposition/composition code is a bit unstable and
        // we work with fixed information that is recomposed when changed,
        // but only decomposed when the entity changes. Note, that all
        // component panels are unique per scene window
        /*
        if (ImGuizmo::IsUsing()) {
            
        }
        */

        auto decomposition = Common::MatrixDecomposition(point.transform);

        vec3 position = decomposition.translation;
        vec3 rotation = decomposition.rotation;
        vec3 scale = decomposition.scale;

        vec3 localPosition = position, localRotation = rotation, localScale = scale;

        ImGui::DragFloat3("Position", &position[0], 0.01f);
        ImGui::DragFloat3("Rotation", &rotation[0], 0.01f);
        ImGui::DragFloat3("Scale", &scale[0], 0.01f, -100.0f, 100.0f);

        // Only recompose when a local change happened
        if (localPosition != position || localScale != scale ||
            localRotation != rotation) {
            Common::MatrixDecomposition composition;
            composition.translation = position;
            composition.rotation = rotation;
            composition.scale = scale;

            point.transform = composition.Compose();
            point.tangent = glm::normalize(glm::vec3(point.transform * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
        }

        ImGui::TreePop();

    }

}