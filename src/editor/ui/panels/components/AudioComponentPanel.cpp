#include "AudioComponentPanel.h"

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool AudioComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AudioComponent &audioComponent) {

        bool resourceChanged = false;

        ResourceHandle<Audio::AudioData> handle;
        if (audioComponent.stream && audioComponent.stream->IsValid())
            handle = audioComponent.stream->data;

        handle = audioSelectionPanel.Render(handle, resourceChanged);

        if (resourceChanged) {
            audioComponent.ChangeResource(handle);
        }

        ImGui::DragFloat("Volume", &audioComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioComponent.cutoff, 0.001f, 0.0001f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Falloff power", &audioComponent.falloffPower, 1.0f, 0.0f, 100.0f);

        ImGui::Checkbox("Play permanently", &audioComponent.permanentPlay);

        if (audioComponent.stream && audioComponent.stream->IsValid()) {
            ImGui::Checkbox("Loop stream", &audioComponent.stream->loop);
        }

        audioSelectionPanel.Reset();

        return resourceChanged;

    }

}