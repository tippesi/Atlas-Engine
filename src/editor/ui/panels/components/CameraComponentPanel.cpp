#include "CameraComponentPanel.h"
#include "../../Widgets.h"

namespace Atlas::Editor::UI {

    bool CameraComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, CameraComponent& cameraComponent) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Main camera", &cameraComponent.isMain);
        ImGui::SetItemTooltip("The main camera renders the scene while playing");

        ImGui::SeparatorText("Transform");

        Widgets::DragVec3("Position", cameraComponent.location, 0.1f, 0.0f, -10000.0f, 10000.0f);

        // Rotation is stored in radians (pitch, yaw), but degrees are easier to work with
        auto rotationDegrees = glm::degrees(cameraComponent.rotation);
        if (ImGui::DragFloat2("Rotation", glm::value_ptr(rotationDegrees), 0.5f, -180.0f, 180.0f, "%.1f\xc2\xb0"))
            cameraComponent.rotation = glm::radians(rotationDegrees);

        ImGui::Checkbox("Use entity translation", &cameraComponent.useEntityTranslation);
        ImGui::Checkbox("Use entity rotation", &cameraComponent.useEntityRotation);

        ImGui::SeparatorText("Third person");

        ImGui::Checkbox("Enabled", &cameraComponent.thirdPerson);
        ImGui::BeginDisabled(!cameraComponent.thirdPerson);
        ImGui::DragFloat("Distance", &cameraComponent.thirdPersonDistance, 0.1f, 0.0f, 100.0f);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Lens");

        ImGui::DragFloat("Field of view", &cameraComponent.fieldOfView, 0.1f, 1.0f, 180.0f);
        ImGui::DragFloat("Aspect ratio", &cameraComponent.aspectRatio, 0.01f, 0.01f, 4.0f);

        ImGui::DragFloat("Near plane", &cameraComponent.nearPlane, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Far plane", &cameraComponent.farPlane, 1.0f, 1.0f, 2000.0f);

        ImGui::DragFloat("Exposure", &cameraComponent.exposure, 0.01f, 0.01f, 100.0f);

        ImGui::PopID();

        return false;

    }

}