#include "RTGIPanel.h"

namespace Atlas::ImguiExtension {

    void RTGIPanel::Render(Ref<Lighting::RTGI>& rtgi, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &rtgi->enable);
        ImGui::Checkbox("Half resolution", &rtgi->halfResolution);

        ImGui::Checkbox("Use shadow map", &rtgi->useShadowMap);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the shadow map to calculate shadows in GI. \
                        This is only possible when cascaded shadow maps are not used.");
        }
        ImGui::Checkbox("Use normal maps", &rtgi->useNormalMaps);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the normal maps to cast GI rays.");
        }
        ImGui::Checkbox("Use DDGI as secondary bounce", &rtgi->ddgi);
        ImGui::Checkbox("Opacity check", &rtgi->opacityCheck);
        // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
        ImGui::SliderFloat("Radiance limit", &rtgi->radianceLimit, 0.0f, 10.0f);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Limits the amount of incoming radiance. Is internally scale by the camera exposure.");
        }
        ImGui::SliderFloat("Bias", &rtgi->bias, 0.0f, 1.0f);
        ImGui::SliderInt("Texture level##RTGI", &rtgi->textureLevel, 0, 10);
        ImGui::SliderInt("Light sample count##RTGI", &rtgi->lightSampleCount, 0, 10);
        ImGui::Text("Denoiser");
        ImGui::SliderFloat("Spatial filter strength", &rtgi->spatialFilterStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Temporal weight", &rtgi->temporalWeight, 0.0f, 1.0f);
        ImGui::SliderFloat("Maximum history clip factor", &rtgi->historyClipMax, 0.0f, 1.0f);
        ImGui::SliderFloat("Current clip factor", &rtgi->currentClipFactor, 0.0f, 10.0f);

        ImGui::PopID();

    }

}