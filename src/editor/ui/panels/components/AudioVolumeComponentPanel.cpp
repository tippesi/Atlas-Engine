#include "AudioVolumeComponentPanel.h"

#include "resource/ResourceManager.h"

namespace Atlas::Editor::UI {

    bool AudioVolumeComponentPanel::Render(Scene::Entity entity, AudioVolumeComponent &audioVolumeComponent) {

        bool resourceChanged = false;

        Ref<Resource<Audio::AudioData>> resource;
        if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid())
            resource = audioVolumeComponent.stream->data.GetResource();
        auto buttonName = resource != nullptr ? resource->GetFileName() : "Drop resource here";

        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        if (ImGui::BeginDragDropTarget()) {
            if (auto dropPayload = ImGui::AcceptDragDropPayload(typeid(Audio::AudioData).name())) {
                Resource<Audio::AudioData>* dropResource;
                std::memcpy(&dropResource, dropPayload->Data, dropPayload->DataSize);
                // We know this resource is loaded, so we can just request a handle without loading
                audioVolumeComponent.ChangeResource(ResourceManager<Audio::AudioData>::GetResource(dropResource->path));
                resourceChanged = true;
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::Text("AABB");
        ImGui::DragFloat3("Min", glm::value_ptr(audioVolumeComponent.aabb.min), 0.1f, -10000.0f, 10000.0f);
        ImGui::DragFloat3("Max", glm::value_ptr(audioVolumeComponent.aabb.max), 0.1f, -10000.0f, 10000.0f);

        ImGui::DragFloat("Volume", &audioVolumeComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioVolumeComponent.cutoff, 0.001f, 0.0f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioVolumeComponent.falloffFactor, 0.1f, 0.0f, 100.0f);

        return resourceChanged;

    }

}