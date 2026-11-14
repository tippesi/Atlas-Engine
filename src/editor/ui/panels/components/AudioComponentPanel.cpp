#include "AudioComponentPanel.h"
#include "../../Widgets.h"

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

        ImGui::SeparatorText("Playback");

        if (Widgets::BeginProperties("AudioPlayback")) {
            Widgets::PropertyLabel("Volume");
            ImGui::SliderFloat("##Volume", &audioComponent.volume, 0.0f, 1.0f);
            Widgets::PropertyLabel("Play permanently");
            ImGui::Checkbox("##Play permanently", &audioComponent.permanentPlay);

            if (audioComponent.stream && audioComponent.stream->IsValid()) {
                Widgets::PropertyLabel("Loop stream");
                ImGui::Checkbox("##Loop stream", &audioComponent.stream->loop);
            }
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Attenuation");

        if (Widgets::BeginProperties("AudioAttenuation")) {
            Widgets::PropertyLabel("Cutoff");
            ImGui::DragFloat("##Cutoff", &audioComponent.cutoff, 0.001f, 0.0001f, 0.2f);
            Widgets::PropertyLabel("Falloff factor");
            ImGui::DragFloat("##Falloff factor", &audioComponent.falloffFactor, 0.05f, 0.0f, 100.0f);
            Widgets::PropertyLabel("Falloff power");
            ImGui::DragFloat("##Falloff power", &audioComponent.falloffPower, 1.0f, 0.0f, 100.0f);
            Widgets::EndProperties();
        }

        audioSelectionPanel.Reset();

        return resourceChanged;

    }

}