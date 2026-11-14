#include "AnimationComponentPanel.h"

#include "../../Widgets.h"
#include "../../../Theme.h"

namespace Atlas::Editor::UI {

    bool AnimationComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AnimationComponent& animationComponent) {

        bool resourceChanged = false;

        auto animation = animationSelectionPanel.Render(animationComponent.animation, resourceChanged);
        if (resourceChanged)
            animationComponent.Play(animation, blendTime);

        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        auto skinned = meshComponent && meshComponent->mesh.IsLoaded() && meshComponent->mesh->data.IsSkinned();
        if (!skinned)
            ImGui::TextDisabled("The entity needs a skinned mesh");

        ImGui::SeparatorText("Playback");

        if (Widgets::BeginProperties("Playback")) {
            Widgets::PropertyLabel("Playing");
            ImGui::Checkbox("##Playing", &animationComponent.playing);
            Widgets::PropertyLabel("Loop");
            ImGui::Checkbox("##Loop", &animationComponent.loop);
            Widgets::PropertyLabel("Speed");
            ImGui::DragFloat("##Speed", &animationComponent.speed, 0.01f, -10.0f, 10.0f);
            Widgets::PropertyLabel("Blend time", "Time to blend over when a new animation is selected");
            ImGui::DragFloat("##Blend time", &blendTime, 0.01f, 0.0f, 5.0f);

            auto duration = animationComponent.animation.IsLoaded() ? animationComponent.animation->duration : 0.0f;
            Widgets::PropertyLabel("Time");
            ImGui::SliderFloat("##Time", &animationComponent.time, 0.0f, duration);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Layers");

        const Mesh::Skeleton* skeleton = skinned ? &meshComponent->mesh->data.skeleton : nullptr;
        int32_t removedLayer = -1;
        for (int32_t i = 0; i < animationComponent.GetLayerCount(); i++) {
            ImGui::PushID(i);
            bool remove = false;
            if (Widgets::RemovableSectionHeader(("Layer " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen,
                remove, "Remove layer"))
                resourceChanged |= RenderLayer(animationComponent.GetLayer(i), skeleton);
            if (remove)
                removedLayer = i;
            ImGui::PopID();
        }

        if (removedLayer >= 0)
            animationComponent.layers.erase(animationComponent.layers.begin() + removedLayer);

        Theme::PushPrimaryButton();
        if (ImGui::Button("Add layer", ImVec2(-FLT_MIN, 0.0f)))
            animationComponent.GetLayer(animationComponent.GetLayerCount());
        Theme::PopPrimaryButton();

        ImGui::SeparatorText("Inverse kinematics");

        int32_t removedChain = -1;
        for (int32_t i = 0; i < animationComponent.GetIKChainCount(); i++) {
            ImGui::PushID(1000 + i);
            bool remove = false;
            if (Widgets::RemovableSectionHeader(("Chain " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen,
                remove, "Remove chain"))
                RenderIKChain(animationComponent.GetIKChain(i), skeleton);
            if (remove)
                removedChain = i;
            ImGui::PopID();
        }

        if (removedChain >= 0)
            animationComponent.ikChains.erase(animationComponent.ikChains.begin() + removedChain);

        Theme::PushPrimaryButton();
        if (ImGui::Button("Add chain", ImVec2(-FLT_MIN, 0.0f)))
            animationComponent.GetIKChain(animationComponent.GetIKChainCount());
        Theme::PopPrimaryButton();

        animationSelectionPanel.Reset();

        return resourceChanged;

    }

    bool AnimationComponentPanel::RenderLayer(AnimationLayer& layer, const Mesh::Skeleton* skeleton) {

        bool resourceChanged = false;

        auto animation = animationSelectionPanel.Render(layer.animation, resourceChanged);
        if (resourceChanged)
            layer.Play(animation, blendTime);

        if (!Widgets::BeginProperties("Layer"))
            return resourceChanged;

        const char* modes[] = { "Override", "Additive" };
        auto mode = static_cast<int32_t>(layer.mode);
        Widgets::PropertyLabel("Mode");
        if (Widgets::SegmentedControl("##Mode", mode, modes, IM_ARRAYSIZE(modes)))
            layer.mode = static_cast<AnimationBlendMode>(mode);

        auto weight = layer.GetTargetWeight();
        Widgets::PropertyLabel("Weight");
        if (ImGui::SliderFloat("##Weight", &weight, 0.0f, 1.0f))
            layer.FadeTo(weight);

        // The mask starts at a joint of the skeleton, the whole skeleton without one
        auto maskRoot = layer.GetMaskRoot();
        auto blendJoints = layer.GetMaskBlendJoints();
        auto maskChanged = false;
        Widgets::PropertyLabel("Mask root");
        if (ImGui::BeginCombo("##Mask root", maskRoot.empty() ? "Whole skeleton" : maskRoot.c_str())) {
            if (ImGui::Selectable("Whole skeleton", maskRoot.empty())) {
                maskRoot.clear();
                maskChanged = true;
            }
            for (size_t i = 0; skeleton && i < skeleton->joints.size(); i++) {
                const auto& name = skeleton->joints[i].name;
                // Skeletons can have several joints with the same name
                ImGui::PushID(int32_t(i));
                if (ImGui::Selectable(name.c_str(), name == maskRoot)) {
                    maskRoot = name;
                    maskChanged = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (!maskRoot.empty()) {
            Widgets::PropertyLabel("Blend joints", "Joints from the mask root down over which the layer fades in, e.g. along the spine");
            maskChanged |= ImGui::DragInt("##Blend joints", &blendJoints, 0.1f, 0, 16);
        }
        if (maskChanged)
            layer.SetMask(maskRoot, blendJoints);

        Widgets::PropertyLabel("Playing");
        ImGui::Checkbox("##Playing", &layer.playing);
        Widgets::PropertyLabel("Loop");
        ImGui::Checkbox("##Loop", &layer.loop);
        Widgets::PropertyLabel("Speed");
        ImGui::DragFloat("##Speed", &layer.speed, 0.01f, -10.0f, 10.0f);

        auto duration = layer.animation.IsLoaded() ? layer.animation->duration : 0.0f;
        Widgets::PropertyLabel("Time");
        ImGui::SliderFloat("##Time", &layer.time, 0.0f, duration);

        Widgets::EndProperties();

        return resourceChanged;

    }

    void AnimationComponentPanel::RenderIKChain(AnimationIKChain& chain, const Mesh::Skeleton* skeleton) {

        std::string joints[] = { chain.GetUpperJoint(), chain.GetMiddleJoint(), chain.GetEndJoint() };
        const char* labels[] = { "Upper joint", "Middle joint", "End joint" };

        if (!Widgets::BeginProperties("Chain"))
            return;

        auto jointsChanged = false;
        for (int32_t i = 0; i < 3; i++) {
            Widgets::PropertyLabel(labels[i]);
            ImGui::PushID(i);
            if (!ImGui::BeginCombo("##Joint", joints[i].empty() ? "None" : joints[i].c_str())) {
                ImGui::PopID();
                continue;
            }
            for (size_t j = 0; skeleton && j < skeleton->joints.size(); j++) {
                const auto& name = skeleton->joints[j].name;
                // Skeletons can have several joints with the same name
                ImGui::PushID(int32_t(j));
                if (ImGui::Selectable(name.c_str(), name == joints[i])) {
                    joints[i] = name;
                    jointsChanged = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
            ImGui::PopID();
        }
        if (jointsChanged)
            chain.SetJoints(joints[0], joints[1], joints[2]);

        Widgets::EndProperties();

        if (!chain.IsValid() && !joints[0].empty())
            ImGui::TextDisabled("Joints not found in the skeleton");

        if (Widgets::BeginProperties("Chain target")) {
            Widgets::PropertyLabel("Weight");
            ImGui::SliderFloat("##Weight", &chain.weight, 0.0f, 1.0f);
            Widgets::PropertyLabel("Relative target", "The target is a world offset from where the animation puts the end joint");
            ImGui::Checkbox("##Relative target", &chain.relativeTarget);
            Widgets::PropertyLabel("Target", chain.relativeTarget ? "World offset from the animated end joint" : "World position the end joint reaches for");
            Widgets::DragVec3("##Target", chain.target, 0.01f);
            Widgets::PropertyLabel("Pole", "Direction in mesh space the middle joint bends to when the chain is straight");
            Widgets::DragVec3("##Pole", chain.pole, 0.01f, 0.0f, -1.0f, 1.0f);
            Widgets::PropertyLabel("Normal");
            Widgets::DragVec3("##Normal", chain.normal, 0.01f, 0.0f, -1.0f, 1.0f);
            Widgets::PropertyLabel("Normal weight");
            ImGui::SliderFloat("##Normal weight", &chain.normalWeight, 0.0f, 1.0f);
            Widgets::EndProperties();
        }

        if (ImGui::Button("Target from animation"))
            chain.target = chain.relativeTarget ? vec3(0.0f) : chain.GetAnimatedEndPosition();

    }

}
