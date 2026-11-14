#include "PlayerComponentPanel.h"
#include "../../../Theme.h"
#include "../../Widgets.h"
#include "Notifications.h"

namespace Atlas::Editor::UI {

	using namespace Physics;

	bool PlayerComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity, PlayerComponent& playerComponent) {

		ImGui::PushID(GetNameID());

		if (!entity.HasComponent<TransformComponent>())
            Widgets::Callout("Player component needs a transform component to work properly", Theme::Warning());

		ImGui::SeparatorText("Shape");

		RenderShapeSettings(entity, playerComponent, *playerComponent.creationSettings);

		ImGui::SeparatorText("Player");

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

		if (transformComponent)
			capsuleSettings->scale = transformComponent->Decompose().scale;

		if (Widgets::BeginProperties("PlayerShape")) {
			Widgets::PropertyLabel("Radius");
			ImGui::DragFloat("##Radius", &capsuleSettings->radius, 0.1f, 0.01f, 100.0f);
			Widgets::PropertyLabel("Height");
			ImGui::DragFloat("##Height", &capsuleSettings->height, 0.1f, 0.01f, 100.0f);
			Widgets::PropertyLabel("Scale");
			Widgets::DragVec3("##Scale", capsuleSettings->scale, 0.01f, 1.0f);
			Widgets::EndProperties();
		}

		if (ImGui::Button("Generate shape", { -FLT_MIN, 0 })) {
			if (!shape->TryCreate()) {
				Notifications::Push({ .message = "Error creating player shape", .type = NotificationType::Error });
			}
			else {
				playerComponent.SetShape(shape);
			}
		}

	}

	void PlayerComponentPanel::RenderPlayerSettings(Scene::Entity, PlayerComponent& player) {

		if (Widgets::BeginProperties("PlayerGeneral")) {
			Widgets::PropertyLabel("Mass");
			ImGui::DragFloat("##Mass", &player.creationSettings->mass, 0.5f, 1.0f, 1000.0f);
			Widgets::PropertyLabel("Max strength");
			ImGui::DragFloat("##Max strength", &player.creationSettings->maxStrength, 0.5f, 1.0f, 1000.0f);
			Widgets::PropertyLabel("Can push bodies");
			ImGui::Checkbox("##Can push bodies", &player.creationSettings->canPushBodies);
			Widgets::EndProperties();
		}

		ImGui::SeparatorText("Movement");

		if (Widgets::BeginProperties("PlayerMovement")) {
			Widgets::PropertyLabel("Slow movement velocity");
			ImGui::DragFloat("##Slow movement velocity", &player.slowVelocity, 0.1f, 0.0f, 100.0f);
			Widgets::PropertyLabel("Fast movement velocity");
			ImGui::DragFloat("##Fast movement velocity", &player.fastVelocity, 0.1f, 0.0f, 100.0f);
			Widgets::PropertyLabel("Jump velocity");
			ImGui::DragFloat("##Jump velocity", &player.jumpVelocity, 0.1f, 0.0f, 100.0f);
			Widgets::PropertyLabel("Acceleration");
			ImGui::DragFloat("##Acceleration", &player.acceleration, 0.5f, 0.0f, 200.0f);
			Widgets::PropertyLabel("Deceleration");
			ImGui::DragFloat("##Deceleration", &player.deceleration, 0.5f, 0.0f, 200.0f);
			Widgets::EndProperties();
		}

		ImGui::Separator();
		if (Widgets::BeginProperties("PlayerCrouch")) {
			Widgets::PropertyLabel("Crouch height");
			ImGui::DragFloat("##Crouch height", &player.crouchHeight, 0.01f, 0.0f, 10.0f);
			Widgets::PropertyLabel("Crouch velocity");
			ImGui::DragFloat("##Crouch velocity", &player.crouchVelocity, 0.1f, 0.0f, 100.0f);
			Widgets::EndProperties();
		}

		ImGui::Separator();
		if (Widgets::BeginProperties("PlayerGround")) {
			Widgets::PropertyLabel("Stick to ground distance");
			ImGui::DragFloat("##Stick to ground distance", &player.stickToGroundDistance, 0.01f, 0.0f, 10.0f);
			Widgets::PropertyLabel("Step up distance");
			ImGui::DragFloat("##Step up distance", &player.walkStairStepUpDistance, 0.01f, 0.0f, 10.0f);
			Widgets::PropertyLabel("Allow input");
			ImGui::Checkbox("##Allow input", &player.allowInput);
			Widgets::EndProperties();
		}

	}

}