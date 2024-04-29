#include "ReflectionPanel.h"

namespace Atlas::ImguiExtension {

    void ReflectionPanel::Render(Ref<Lighting::Reflection> &reflection, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        bool halfRes = target->GetReflectionResolution() == Renderer::RenderResolution::HALF_RES;

        ImGui::Checkbox("Enable", &reflection->enable);
        ImGui::Checkbox("Half resolution", &halfRes);

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
        ImGui::Checkbox("Enable GI in reflection", &reflection->gi);
        ImGui::Checkbox("Opacity check", &reflection->opacityCheck);
        // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
        ImGui::SliderFloat("Radiance Limit", &reflection->radianceLimit, 0.0f, 10.0f);
        ImGui::SliderFloat("Bias", &reflection->bias, 0.0f, 1.0f);
        ImGui::SliderInt("Texture level##Reflection", &reflection->textureLevel, 0, 10);
        ImGui::Text("Denoiser");
        ImGui::SliderFloat("Spatial filter strength", &reflection->spatialFilterStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Temporal weight", &reflection->temporalWeight, 0.0f, 1.0f);
        ImGui::SliderFloat("Maximum history clip factor", &reflection->historyClipMax, 0.0f, 1.0f);
        ImGui::SliderFloat("Current clip factor", &reflection->currentClipFactor, 0.0f, 10.0f);

        ImGui::PopID();

        if (halfRes && target->GetReflectionResolution() != Renderer::RenderResolution::HALF_RES) {
            target->SetReflectionResolution(Renderer::RenderResolution::HALF_RES);
        }
        else if (!halfRes && target->GetReflectionResolution() == Renderer::RenderResolution::HALF_RES) {
            target->SetReflectionResolution(Renderer::RenderResolution::FULL_RES);
        }

    }

}