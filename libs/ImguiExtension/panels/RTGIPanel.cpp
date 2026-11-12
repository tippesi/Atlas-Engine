#include "RTGIPanel.h"

namespace Atlas::ImguiExtension {

    void RTGIPanel::Render(Ref<Lighting::RTGI>& rtgi, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("RTGIGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &rtgi->enable);
            UIElements::PropertyLabel("Half resolution");
            ImGui::Checkbox("##Half resolution", &rtgi->halfResolution);

            UIElements::PropertyLabel("Use normal maps");
            ImGui::Checkbox("##Use normal maps", &rtgi->useNormalMaps);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Uses the normal maps to cast GI rays.");
            }
            UIElements::PropertyLabel("Use DDGI as secondary bounce");
            ImGui::Checkbox("##Use DDGI as secondary bounce", &rtgi->ddgi);
            UIElements::PropertyLabel("Opacity check");
            ImGui::Checkbox("##Opacity check", &rtgi->opacityCheck);
            // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
            UIElements::PropertyLabel("Radiance limit");
            ImGui::DragFloat("##Radiance limit", &rtgi->radianceLimit, 0.1f, 0.0f, 1000.0f);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Limits the amount of incoming radiance. Is internally scale by the camera exposure.");
            }
            UIElements::PropertyLabel("Bias");
            ImGui::SliderFloat("##Bias", &rtgi->bias, 0.0f, 1.0f);
            UIElements::PropertyLabel("Texture level");
            ImGui::SliderInt("##Texture level", &rtgi->textureLevel, 0, 10);
            UIElements::PropertyLabel("Sample count");
            ImGui::SliderInt("##Sample count", &rtgi->sampleCount, 1, 10);
            UIElements::PropertyLabel("Light sample count");
            ImGui::SliderInt("##Light sample count", &rtgi->lightSampleCount, 1, 10);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Denoiser");
        if (UIElements::BeginProperties("RTGIDenoiser")) {
            UIElements::PropertyLabel("Spatial filter strength");
            ImGui::SliderFloat("##Spatial filter strength", &rtgi->spatialFilterStrength, 0.0f, 10.0f);
            UIElements::PropertyLabel("Temporal weight");
            ImGui::SliderFloat("##Temporal weight", &rtgi->temporalWeight, 0.0f, 1.0f);
            UIElements::PropertyLabel("Maximum history clip factor");
            ImGui::SliderFloat("##Maximum history clip factor", &rtgi->historyClipMax, 0.0f, 1.0f);
            UIElements::PropertyLabel("Current clip factor");
            ImGui::SliderFloat("##Current clip factor", &rtgi->currentClipFactor, 0.0f, 10.0f);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
