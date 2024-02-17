#include "AudioComponentPanel.h"

#include "resource/ResourceManager.h"

namespace Atlas::Editor::UI {

    bool AudioComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AudioComponent &audioComponent) {

        bool resourceChanged = false;

        Ref<Resource<Audio::AudioData>> resource;
        if (audioComponent.stream && audioComponent.stream->IsValid())
            resource = audioComponent.stream->data.GetResource();
        auto buttonName = resource != nullptr ? resource->GetFileName() : "Drop resource here";

        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        if (ImGui::BeginDragDropTarget()) {
            if (auto dropPayload = ImGui::AcceptDragDropPayload(typeid(Audio::AudioData).name())) {
                Resource<Audio::AudioData>* dropResource;
                std::memcpy(&dropResource, dropPayload->Data, dropPayload->DataSize);
                // We know this resource is loaded, so we can just request a handle without loading
                audioComponent.ChangeResource(ResourceManager<Audio::AudioData>::GetResource(dropResource->path));
                resourceChanged = true;
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::DragFloat("Volume", &audioComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioComponent.cutoff, 0.001f, 0.0f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioComponent.falloffFactor, 0.05f, 0.0f, 100.0f);

        if (audioComponent.stream && audioComponent.stream->IsValid()) {
            ImGui::Checkbox("Loop stream", &audioComponent.stream->loop);
        }

        return resourceChanged;

    }

}