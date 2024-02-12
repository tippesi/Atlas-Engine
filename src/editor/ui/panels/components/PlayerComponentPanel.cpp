#include "PlayerComponentPanel.h"
#include "Notifications.h"

namespace Atlas::Editor::UI {

	using namespace Physics;

	bool PlayerComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, PlayerComponent& playerComponent) {

		ImGui::PushID(GetNameID());

		RenderShapeSettings(entity, *playerComponent.playerCreationSettings);

		ImGui::PopID();

		return false;

	}

	void PlayerComponentPanel::RenderShapeSettings(Scene::Entity entity, Physics::PlayerCreationSettings& creationSettings) {

		auto& shape = creationSettings.shape;

		auto transformComponent = entity.TryGetComponent<TransformComponent>();

		if (!shape->settings)
			shape->settings = CreateRef<CapsuleShapeSettings>();

		auto capsuleSettings = static_cast<CapsuleShapeSettings*>(shape->settings.get());

		ImGui::DragFloat("Radius", &capsuleSettings->radius, 0.1f, 0.01f, 100.0f);
		ImGui::DragFloat("Height", &capsuleSettings->height, 0.1f, 0.01f, 100.0f);

		if (transformComponent)
			capsuleSettings->scale = transformComponent->Decompose().scale;
		ImGui::DragFloat3("Scale", glm::value_ptr(capsuleSettings->scale), 0.01f, 0.0f);

		if (ImGui::Button("Generate shape", { -FLT_MIN, 0 }))
			if (!shape->TryCreate())
				Notifications::Push({ .message = "Error creating player shape", .color = vec3(1.0f, 0.0f, 0.0f) });

	}

}