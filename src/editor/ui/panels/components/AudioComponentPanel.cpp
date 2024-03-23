#include "AudioComponentPanel.h"

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool AudioComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AudioComponent &audioComponent) {

        bool resourceChanged = false;

        Ref<Resource<Audio::AudioData>> resource;
        if (audioComponent.stream && audioComponent.stream->IsValid())
            resource = audioComponent.stream->data.GetResource();
        auto buttonName = resource != nullptr ? resource->GetFileName() : "Drop resource here";

        if (ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0}))
            audioSelectionPopup.Open();

        // Such that drag and drop will work from the content browser
        if (ImGui::IsDragDropActive() && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
            ImGui::SetWindowFocus();
            ImGui::SetItemDefaultFocus();
        }

        auto handle = ResourcePayloadHelper::AcceptDropResource<Audio::AudioData>();

        auto audioResources = ResourceManager<Audio::AudioData>::GetResources();
        handle = audioSelectionPopup.Render(audioResources);

        if (handle.IsValid()) {
            audioComponent.ChangeResource(handle);
            resourceChanged = true;
        }

        ImGui::DragFloat("Volume", &audioComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioComponent.cutoff, 0.001f, 0.0001f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Falloff power", &audioComponent.falloffPower, 1.0f, 0.0f, 100.0f);

        if (audioComponent.stream && audioComponent.stream->IsValid()) {
            ImGui::Checkbox("Loop stream", &audioComponent.stream->loop);
        }

        return resourceChanged;

    }

}