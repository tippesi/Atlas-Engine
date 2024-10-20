#include "CameraComponentPanel.h"

namespace Atlas::Editor::UI {

    bool CameraComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, CameraComponent& cameraComponent) {

        ImGui::PushID(GetNameID());

        ImGui::Text("Transform");

        ImGui::DragFloat3("Position", glm::value_ptr(cameraComponent.location), 0.1f, -10000.0f, 10000.0f);
        ImGui::DragFloat2("Rotation", glm::value_ptr(cameraComponent.rotation), 0.01f, -glm::pi<float>(), glm::pi<float>());

        ImGui::Checkbox("Use entity translation", &cameraComponent.useEntityTranslation);
        ImGui::Checkbox("Use entity rotation", &cameraComponent.useEntityRotation);

        ImGui::Checkbox("Third person", &cameraComponent.thirdPerson);
        ImGui::DragFloat("Third person distance", &cameraComponent.thirdPersonDistance, 0.1f, 0.0f, 100.0f);

        ImGui::Checkbox("Main", &cameraComponent.isMain);

        ImGui::Text("Lens properties");

        ImGui::DragFloat("Field of view", &cameraComponent.fieldOfView, 0.1f, 1.0f, 180.0f);
        ImGui::DragFloat("Aspect ratio", &cameraComponent.aspectRatio, 0.01f, 0.01f, 4.0f);

        ImGui::DragFloat("Near plane", &cameraComponent.nearPlane, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Far plane", &cameraComponent.farPlane, 1.0f, 1.0f, 2000.0f);

        ImGui::DragFloat("Exposure", &cameraComponent.exposure, 0.01f, 0.01f, 100.0f),

        ImGui::PopID();

        return false;

    }

}