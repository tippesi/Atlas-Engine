#include "AnimationComponentPanel.h"

namespace Atlas::Editor::UI {

    bool AnimationComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AnimationComponent& animationComponent) {

        bool resourceChanged = false;

        auto animation = animationSelectionPanel.Render(animationComponent.animation, resourceChanged);
        if (resourceChanged)
            animationComponent.Play(animation, blendTime);

        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        if (!meshComponent || !meshComponent->mesh.IsLoaded() || !meshComponent->mesh->data.IsSkinned())
            ImGui::TextDisabled("The entity needs a skinned mesh");

        ImGui::SeparatorText("Playback");

        ImGui::Checkbox("Playing", &animationComponent.playing);
        ImGui::Checkbox("Loop", &animationComponent.loop);
        ImGui::DragFloat("Speed", &animationComponent.speed, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat("Blend time", &blendTime, 0.01f, 0.0f, 5.0f);
        ImGui::SetItemTooltip("Time to blend over when a new animation is selected");

        auto duration = animationComponent.animation.IsLoaded() ? animationComponent.animation->duration : 0.0f;
        ImGui::SliderFloat("Time", &animationComponent.time, 0.0f, duration);

        animationSelectionPanel.Reset();

        return resourceChanged;

    }

}
