#include "ReflectionPanel.h"

#include "graphics/GraphicsDevice.h"

namespace Atlas::ImguiExtension {

    void ReflectionPanel::Render(Ref<Lighting::Reflection> &reflection, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("ReflectionGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &reflection->enable);
            UIElements::PropertyLabel("Enable RT");
            ImGui::Checkbox("##Enable RT", &reflection->rt);
            UIElements::PropertyLabel("Enable SSR");
            ImGui::Checkbox("##Enable SSR", &reflection->ssr);
            UIElements::PropertyLabel("Half resolution");
            ImGui::Checkbox("##Half resolution", &reflection->halfResolution);
            UIElements::PropertyLabel("Upsample before filtering");
            ImGui::Checkbox("##Upsample before filtering", &reflection->upsampleBeforeFiltering);

            //ImGui::Checkbox("Enable raytracing##Reflection", &reflection->rt);
            UIElements::PropertyLabel("Use normal maps");
            ImGui::Checkbox("##Use normal maps", &reflection->useNormalMaps);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Uses the normal maps to cast reflection rays.");
            }
            UIElements::PropertyLabel("Use DDGI in reflection");
            ImGui::Checkbox("##Use DDGI in reflection", &reflection->ddgi);
            UIElements::PropertyLabel("Opacity check");
            ImGui::Checkbox("##Opacity check", &reflection->opacityCheck);
            auto reorderSupported = Graphics::GraphicsDevice::DefaultDevice->support.shaderExecutionReordering;
            UIElements::PropertyLabel("Shader execution reordering");
            ImGui::BeginDisabled(!reorderSupported);
            ImGui::Checkbox("##Shader execution reordering", &reflection->shaderExecutionReordering);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Sorts the rays by the mesh they hit before shading. Helps with many different meshes, "
                    "costs with few. Needs hit objects, which the device %s.", reorderSupported ? "supports" : "doesn't support");
            }
            ImGui::EndDisabled();
            // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
            UIElements::PropertyLabel("Radiance limit");
            ImGui::DragFloat("##Radiance limit", &reflection->radianceLimit, 0.1f, 0.0f, 1000.0f);
            UIElements::PropertyLabel("Bias");
            ImGui::SliderFloat("##Bias", &reflection->bias, 0.0f, 1.0f);
            UIElements::PropertyLabel("Roughness cuttoff");
            ImGui::SliderFloat("##Roughness cuttoff", &reflection->roughnessCutoff, 0.0f, 1.0f);
            UIElements::PropertyLabel("Texture level");
            ImGui::SliderInt("##Texture level", &reflection->textureLevel, 0, 10);
            UIElements::PropertyLabel("Sample count");
            ImGui::SliderInt("##Sample count", &reflection->sampleCount, 1, 10);
            UIElements::PropertyLabel("Light sample count");
            ImGui::SliderInt("##Light sample count", &reflection->lightSampleCount, 1, 10);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Denoiser");
        if (UIElements::BeginProperties("ReflectionDenoiser")) {
            UIElements::PropertyLabel("Spatial filter strength");
            ImGui::SliderFloat("##Spatial filter strength", &reflection->spatialFilterStrength, 0.0f, 10.0f);
            UIElements::PropertyLabel("Temporal weight");
            ImGui::SliderFloat("##Temporal weight", &reflection->temporalWeight, 0.0f, 1.0f);
            UIElements::PropertyLabel("Maximum history clip factor");
            ImGui::SliderFloat("##Maximum history clip factor", &reflection->historyClipMax, 0.0f, 1.0f);
            UIElements::PropertyLabel("Current clip factor");
            ImGui::SliderFloat("##Current clip factor", &reflection->currentClipFactor, 0.0f, 10.0f);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
