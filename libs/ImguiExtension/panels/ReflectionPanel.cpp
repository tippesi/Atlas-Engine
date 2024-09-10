#include "ReflectionPanel.h"

namespace Atlas::ImguiExtension {

    void ReflectionPanel::Render(Ref<Lighting::Reflection> &reflection, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &reflection->enable);
        ImGui::Checkbox("Half resolution", &reflection->halfResolution);
        ImGui::Checkbox("Upsample before filtering", &reflection->upsampleBeforeFiltering);

        //ImGui::Checkbox("Enable raytracing##Reflection", &reflection->rt);
        ImGui::Checkbox("Use shadow map", &reflection->useShadowMap);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the shadow map to calculate shadows in reflections. \
                        This is only possible when cascaded shadow maps are not used.");
        }
        ImGui::Checkbox("Use normal maps", &reflection->useNormalMaps);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the normal maps to cast reflection rays.");
        }
        ImGui::Checkbox("Use DDGI in reflection", &reflection->ddgi);
        ImGui::Checkbox("Opacity check", &reflection->opacityCheck);
        // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
        ImGui::SliderFloat("Radiance limit", &reflection->radianceLimit, 0.0f, 10.0f);
        ImGui::SliderFloat("Bias", &reflection->bias, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness cuttoff", &reflection->roughnessCutoff, 0.0f, 1.0f);
        ImGui::SliderInt("Texture level##Reflection", &reflection->textureLevel, 0, 10);
        ImGui::SliderInt("Sample count##Reflection", &reflection->sampleCount, 1, 10);
        ImGui::SliderInt("Light sample count##Reflection", &reflection->lightSampleCount, 0, 10);
        ImGui::Text("Denoiser");
        ImGui::SliderFloat("Spatial filter strength", &reflection->spatialFilterStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Temporal weight", &reflection->temporalWeight, 0.0f, 1.0f);
        ImGui::SliderFloat("Maximum history clip factor", &reflection->historyClipMax, 0.0f, 1.0f);
        ImGui::SliderFloat("Current clip factor", &reflection->currentClipFactor, 0.0f, 10.0f);

        ImGui::PopID();

    }

}