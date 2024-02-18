#include "SSGIPanel.h"

namespace Atlas::ImguiExtension {

    void SSGIPanel::Render(Ref<Lighting::SSGI>& ssgi) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &ssgi->enable);
        ImGui::Checkbox("Enable ambient occlusion", &ssgi->enableAo);
        ImGui::SliderInt("Ray count", &ssgi->rayCount, 1, 8);
        ImGui::SliderInt("Sample count", &ssgi->sampleCount, 1, 16);
        ImGui::SliderFloat("Radius", &ssgi->radius, 0.0f, 10.0f);
        ImGui::SliderFloat("Ao strength", &ssgi->aoStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Irradiance limit", &ssgi->irradianceLimit, 0.0f, 10.0f);

        ImGui::PopID();

    }

}