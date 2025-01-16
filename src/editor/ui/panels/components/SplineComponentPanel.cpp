#include "SplineComponentPanel.h"

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
        for (auto& controlPoint : controlPoints) {
            RenderControlPoint(controlPoint, counter++);
        }

        auto region = ImGui::GetContentRegionAvail();
        if (ImGui::Button("Add point", ImVec2(region.x, 0.0f))) {
            auto controlPoint = controlPoints.empty() ? SplineControlPoint{} : controlPoints.back();
            controlPoints.push_back(controlPoint);
        }

        ImGui::PopID();

        return false;

    }

    bool SplineComponentPanel::RenderControlPoint(SplineControlPoint& point, size_t idx) {

        bool open = ImGui::TreeNode((void*)idx, "Control point %d", int32_t(idx));
        if (!open)
            return false;

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

        return false;

    }

}