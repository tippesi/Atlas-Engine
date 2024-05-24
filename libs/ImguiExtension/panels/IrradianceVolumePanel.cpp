#include "IrradianceVolumePanel.h"
#include "StringHelper.h"

namespace Atlas::ImguiExtension {

    void IrradianceVolumePanel::Render(Ref<Lighting::IrradianceVolume> &volume) {

        ImGui::PushID(GetNameID());

        ImGui::Text("Probe count: %s", VecToString(volume->probeCount).c_str());
        ImGui::Text("Cell size: %s", VecToString(volume->cellSize[0]).c_str());
        ImGui::Checkbox("Enable volume", &volume->enable);
        ImGui::Checkbox("Update volume", &volume->update);
        ImGui::Checkbox("Visualize probes", &volume->debug);
        ImGui::Checkbox("Sample emissives", &volume->sampleEmissives);
        ImGui::Checkbox("Use shadow map", &volume->useShadowMap);
        ImGui::Checkbox("Opacity check", &volume->opacityCheck);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the shadow map to calculate shadows in reflections. \
                        This is only possible when cascaded shadow maps are not used.");
        }

        const char* gridResItems [] = { "5x5x5", "10x10x10", "20x20x20", "30x30x30" };
        int currentItem = 0;
        if (volume->probeCount == glm::ivec3(5)) currentItem = 0;
        if (volume->probeCount == glm::ivec3(10)) currentItem = 1;
        if (volume->probeCount == glm::ivec3(20)) currentItem = 2;
        if (volume->probeCount == glm::ivec3(30)) currentItem = 3;
        auto prevItem = currentItem;
        ImGui::Combo("Resolution", &currentItem, gridResItems, IM_ARRAYSIZE(gridResItems));

        if (currentItem != prevItem) {
            switch (currentItem) {
                case 0: volume->SetProbeCount(glm::ivec3(5)); break;
                case 1: volume->SetProbeCount(glm::ivec3(10)); break;
                case 2: volume->SetProbeCount(glm::ivec3(20)); break;
                case 3: volume->SetProbeCount(glm::ivec3(30)); break;
            }
        }

        const char* rayCountItems[] = { "32", "64", "128", "256", "512" };
        currentItem = 0;
        if (volume->rayCount == 32) currentItem = 0;
        if (volume->rayCount == 64) currentItem = 1;
        if (volume->rayCount == 128) currentItem = 2;
        if (volume->rayCount == 256) currentItem = 3;
        if (volume->rayCount == 512) currentItem = 4;
        prevItem = currentItem;
        ImGui::Combo("Ray count", &currentItem, rayCountItems, IM_ARRAYSIZE(rayCountItems));

        if (currentItem != prevItem) {
            switch (currentItem) {
                case 0: volume->SetRayCount(32, 32); break;
                case 1: volume->SetRayCount(64, 32); break;
                case 2: volume->SetRayCount(128, 32); break;
                case 3: volume->SetRayCount(256, 32); break;
                case 4: volume->SetRayCount(512, 32); break;
            }
        }

        ImGui::SliderFloat("Strength", &volume->strength, 0.0f, 5.0f);
        ImGui::Separator();
        ImGui::Text("AABB"); 
        /*
        ImGui::DragFloat3("Min", (float*)&volume->aabb.min, 0.5f, -2000.0f, 2000.0f);
        ImGui::DragFloat3("Max", (float*)&volume->aabb.max, 0.5f, -2000.0f, 2000.0f);
        volume->SetAABB(volume->aabb);
        */
        ImGui::Separator();
        ImGui::SliderFloat("Hysteresis", &volume->hysteresis, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Sharpness", &volume->sharpness, 0.01f, 200.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Bias", &volume->bias, 0.0f, 1.0f);
        auto prevGamma = volume->gamma;
        ImGui::SliderFloat("Gamma exponent", &volume->gamma, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        if (prevGamma != volume->gamma) volume->ClearProbes();
        ImGui::Separator();
        if (ImGui::Button("Reset probe offsets")) {
            volume->ResetProbeOffsets();
        }
        ImGui::Checkbox("Optimize probes", &volume->optimizeProbes);

        ImGui::PopID();

    }

}