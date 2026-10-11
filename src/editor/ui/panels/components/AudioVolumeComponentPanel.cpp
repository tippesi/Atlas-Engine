#include "AudioVolumeComponentPanel.h"
#include "../../Widgets.h"

#include "../../../tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

    bool AudioVolumeComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, AudioVolumeComponent &audioVolumeComponent) {

        bool resourceChanged = false;

        ResourceHandle<Audio::AudioData> handle;
        if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid())
            handle = audioVolumeComponent.stream->data;
        
        handle = audioSelectionPanel.Render(handle, resourceChanged);

        if (resourceChanged) {
            audioVolumeComponent.ChangeResource(handle);
        }

        ImGui::Text("AABB");
        Widgets::DragVec3("Min", audioVolumeComponent.aabb.min, 0.1f, 0.0f, -10000.0f, 10000.0f);
        Widgets::DragVec3("Max", audioVolumeComponent.aabb.max, 0.1f, 0.0f, -10000.0f, 10000.0f);

        ImGui::DragFloat("Volume", &audioVolumeComponent.volume, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Cutoff", &audioVolumeComponent.cutoff, 0.001f, 0.0001f, 0.2f);
        ImGui::DragFloat("Falloff factor", &audioVolumeComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Falloff power", &audioVolumeComponent.falloffPower, 1.0f, 0.0f, 100.0f);

        ImGui::Checkbox("Play permanently", &audioVolumeComponent.permanentPlay);

        if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid()) {
            ImGui::Checkbox("Loop stream", &audioVolumeComponent.stream->loop);
        }

        audioSelectionPanel.Reset();

        return resourceChanged;

    }

}