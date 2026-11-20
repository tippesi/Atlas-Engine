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

        if (Widgets::BeginProperties("AudioVolume")) {
            Widgets::PropertyLabel("AABB min");
            Widgets::DragVec3("##AABB min", audioVolumeComponent.aabb.min, 0.1f, 0.0f, -10000.0f, 10000.0f);
            Widgets::PropertyLabel("AABB max");
            Widgets::DragVec3("##AABB max", audioVolumeComponent.aabb.max, 0.1f, 0.0f, -10000.0f, 10000.0f);

            Widgets::PropertyLabel("Volume");
            ImGui::DragFloat("##Volume", &audioVolumeComponent.volume, 0.005f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Cutoff");
            ImGui::DragFloat("##Cutoff", &audioVolumeComponent.cutoff, 0.001f, 0.0001f, 0.2f);
            Widgets::PropertyLabel("Falloff factor");
            ImGui::DragFloat("##Falloff factor", &audioVolumeComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
            Widgets::PropertyLabel("Falloff power");
            ImGui::DragFloat("##Falloff power", &audioVolumeComponent.falloffPower, 1.0f, 0.0f, 100.0f);

            Widgets::PropertyLabel("Play permanently");
            ImGui::Checkbox("##Play permanently", &audioVolumeComponent.permanentPlay);

            if (audioVolumeComponent.stream && audioVolumeComponent.stream->IsValid()) {
                Widgets::PropertyLabel("Loop stream");
                ImGui::Checkbox("##Loop stream", &audioVolumeComponent.stream->loop);
            }
            Widgets::EndProperties();
        }

        audioSelectionPanel.Reset();

        return resourceChanged;

    }

}