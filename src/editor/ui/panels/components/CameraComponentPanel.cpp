#include "CameraComponentPanel.h"
#include "../../Widgets.h"

namespace Atlas::Editor::UI {

    bool CameraComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, CameraComponent& cameraComponent) {

        ImGui::PushID(GetNameID());

        if (Widgets::BeginProperties("CameraComponent")) {
            Widgets::PropertyLabel("Main camera", "The main camera renders the scene while playing");
            ImGui::Checkbox("##Main camera", &cameraComponent.isMain);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Transform");

        if (Widgets::BeginProperties("CameraTransform")) {
            Widgets::PropertyLabel("Position");
            Widgets::DragVec3("##Position", cameraComponent.location, 0.1f, 0.0f, -10000.0f, 10000.0f);

            // Rotation is stored in radians (pitch, yaw), but degrees are easier to work with
            auto rotationDegrees = glm::degrees(cameraComponent.rotation);
            Widgets::PropertyLabel("Rotation");
            if (ImGui::DragFloat2("##Rotation", glm::value_ptr(rotationDegrees), 0.5f, -180.0f, 180.0f, "%.1f\xc2\xb0"))
                cameraComponent.rotation = glm::radians(rotationDegrees);

            Widgets::PropertyLabel("Use entity translation");
            ImGui::Checkbox("##Use entity translation", &cameraComponent.useEntityTranslation);
            Widgets::PropertyLabel("Use entity rotation");
            ImGui::Checkbox("##Use entity rotation", &cameraComponent.useEntityRotation);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Third person");

        if (Widgets::BeginProperties("CameraThirdPerson")) {
            Widgets::PropertyLabel("Enabled");
            ImGui::Checkbox("##Enabled", &cameraComponent.thirdPerson);
            ImGui::BeginDisabled(!cameraComponent.thirdPerson);
            Widgets::PropertyLabel("Distance");
            ImGui::DragFloat("##Distance", &cameraComponent.thirdPersonDistance, 0.1f, 0.0f, 100.0f);
            ImGui::EndDisabled();
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Lens");

        if (Widgets::BeginProperties("CameraLens")) {
            Widgets::PropertyLabel("Field of view");
            ImGui::DragFloat("##Field of view", &cameraComponent.fieldOfView, 0.1f, 1.0f, 180.0f);
            Widgets::PropertyLabel("Aspect ratio");
            ImGui::DragFloat("##Aspect ratio", &cameraComponent.aspectRatio, 0.01f, 0.01f, 4.0f);

            Widgets::PropertyLabel("Near plane");
            ImGui::DragFloat("##Near plane", &cameraComponent.nearPlane, 0.01f, 0.01f, 10.0f);
            Widgets::PropertyLabel("Far plane");
            ImGui::DragFloat("##Far plane", &cameraComponent.farPlane, 1.0f, 1.0f, 2000.0f);

            Widgets::PropertyLabel("Exposure");
            ImGui::DragFloat("##Exposure", &cameraComponent.exposure, 0.01f, 0.01f, 100.0f);
            Widgets::EndProperties();
        }

        ImGui::PopID();

        return false;

    }

}