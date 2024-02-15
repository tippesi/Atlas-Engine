#include "PlayerComponentPanel.h"
#include "Notifications.h"

namespace Atlas::Editor::UI {

	using namespace Physics;

	bool PlayerComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, PlayerComponent& playerComponent) {

		ImGui::PushID(GetNameID());

		ImGui::Text("Shape");

		RenderShapeSettings(entity, playerComponent, *playerComponent.creationSettings);

		ImGui::Separator();

		ImGui::Text("Player");

		RenderPlayerSettings(entity, playerComponent);

		ImGui::PopID();

		return false;

	}

	void PlayerComponentPanel::RenderShapeSettings(Scene::Entity entity, PlayerComponent& playerComponent, 
		Physics::PlayerCreationSettings& creationSettings) {

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
			else
				playerComponent.SetShape(shape);

	}

	void PlayerComponentPanel::RenderPlayerSettings(Scene::Entity, PlayerComponent& player) {

		ImGui::DragFloat("Mass", &player.creationSettings->mass, 0.5f, 1.0f, 1000.0f);
		ImGui::DragFloat("Max strength", &player.creationSettings->maxStrength, 0.5f, 1.0f, 1000.0f);

		ImGui::Separator();

		ImGui::DragFloat("Slow movement velocity", &player.slowVelocity, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Fast movement velocity", &player.fastVelocity, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Jump velocity", &player.jumpVelocity, 0.1f, 0.0f, 100.0f);

	}

}