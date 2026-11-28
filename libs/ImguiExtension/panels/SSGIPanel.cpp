#include "SSGIPanel.h"

namespace Atlas::ImguiExtension {

    void SSGIPanel::Render(Ref<Lighting::SSGI>& ssgi, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("SSGIGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &ssgi->enable);
            UIElements::PropertyLabel("Half resolution");
            ImGui::Checkbox("##Half resolution", &ssgi->halfResolution);

            UIElements::PropertyLabel("Enable ambient occlusion");
            ImGui::Checkbox("##Enable ambient occlusion", &ssgi->enableAo);
            UIElements::PropertyLabel("Ray count");
            ImGui::DragInt("##Ray count", &ssgi->rayCount, 0.1f, 1, 8);
            UIElements::PropertyLabel("Sample count");
            ImGui::DragInt("##Sample count", &ssgi->sampleCount, 0.1f, 1, 16);
            UIElements::PropertyLabel("Radius");
            ImGui::DragFloat("##Radius", &ssgi->radius, 1.0f, 0.0f, 2000.0f);
            UIElements::PropertyLabel("Thickness");
            ImGui::DragFloat("##Thickness", &ssgi->thickness, 0.001f, 0.0f, 0.2f, "%.3f");
            UIElements::PropertyLabel("Ao strength");
            ImGui::DragFloat("##Ao strength", &ssgi->aoStrength, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Irradiance limit");
            ImGui::DragFloat("##Irradiance limit", &ssgi->irradianceLimit, 0.1f, 0.0f, 1000.0f);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
