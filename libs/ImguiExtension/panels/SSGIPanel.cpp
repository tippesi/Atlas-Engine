#include "SSGIPanel.h"

namespace Atlas::ImguiExtension {

    void SSGIPanel::Render(Ref<Lighting::SSGI>& ssgi, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        bool halfRes = target->GetGIResolution() == Renderer::RenderResolution::HALF_RES;

        ImGui::Checkbox("Enable", &ssgi->enable);
        ImGui::Checkbox("Half resolution", &halfRes);

        ImGui::Checkbox("Enable ambient occlusion", &ssgi->enableAo);
        ImGui::SliderInt("Ray count", &ssgi->rayCount, 1, 8);
        ImGui::SliderInt("Sample count", &ssgi->sampleCount, 1, 16);
        ImGui::SliderFloat("Radius", &ssgi->radius, 0.0f, 10.0f);
        ImGui::SliderFloat("Ao strength", &ssgi->aoStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Irradiance limit", &ssgi->irradianceLimit, 0.0f, 10.0f);

        ImGui::PopID();

        if (halfRes && target->GetGIResolution() != Renderer::RenderResolution::HALF_RES) {
            target->SetGIResolution(Renderer::RenderResolution::HALF_RES);
        }
        else if (!halfRes && target->GetGIResolution() == Renderer::RenderResolution::HALF_RES) {
            target->SetGIResolution(Renderer::RenderResolution::FULL_RES);
        }

    }

}