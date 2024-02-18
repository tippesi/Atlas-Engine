#include "AudioVolumeComponentPanel.h"

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool AudioVolumeComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AudioVolumeComponent &audioVolumeComponent) {

        bool resourceChanged = false;

        Ref<Resource<Audio::AudioData>> resource;
        if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid())
            resource = audioVolumeComponent.stream->data.GetResource();
        auto buttonName = resource != nullptr ? resource->GetFileName() : "Drop resource here";

        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        auto handle = ResourcePayloadHelper::AcceptDropResource<Audio::AudioData>();
        if (handle.IsValid()) {
            audioVolumeComponent.ChangeResource(handle);
            resourceChanged = true;
        }

        ImGui::Text("AABB");
        ImGui::DragFloat3("Min", glm::value_ptr(audioVolumeComponent.aabb.min), 0.1f, -10000.0f, 10000.0f);
        ImGui::DragFloat3("Max", glm::value_ptr(audioVolumeComponent.aabb.max), 0.1f, -10000.0f, 10000.0f);

        ImGui::DragFloat("Volume", &audioVolumeComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioVolumeComponent.cutoff, 0.001f, 0.0001f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioVolumeComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Falloff power", &audioVolumeComponent.falloffPower, 1.0f, 0.0f, 100.0f);

        if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid()) {
            ImGui::Checkbox("Loop stream", &audioVolumeComponent.stream->loop);
        }

        return resourceChanged;

    }

}