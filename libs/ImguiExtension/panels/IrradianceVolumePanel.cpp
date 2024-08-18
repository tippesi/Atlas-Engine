#include "IrradianceVolumePanel.h"
#include "StringHelper.h"

#include <scene/Scene.h>

namespace Atlas::ImguiExtension {

    void IrradianceVolumePanel::Render(Ref<Lighting::IrradianceVolume> &volume, Ref<Scene::Scene>& scene) {

        ImGui::PushID(GetNameID());

        ImGui::Text("Probe count: %s", VecToString(volume->probeCount).c_str());
        ImGui::Text("Cell size: %s", VecToString(volume->cascades[0].cellSize).c_str());
        ImGui::Checkbox("Enable volume", &volume->enable);
        ImGui::Checkbox("Update volume", &volume->update);
        ImGui::Checkbox("Visualize probes", &volume->debug);
        ImGui::Checkbox("Sample emissives", &volume->sampleEmissives);
        ImGui::Checkbox("Use shadow map", &volume->useShadowMap);
        ImGui::Checkbox("Calculate visibility", &volume->visibility);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Uses the shadow map to calculate shadows in the GI. \
                        This is only possible when cascaded shadow maps are not used.");
        }

        ImGui::Checkbox("Opacity check", &volume->opacityCheck);
        ImGui::Checkbox("Scrolling", &volume->scroll);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Scrolls the volume based on a set position (by default the camera). \
                        This should be used for large scenes where the volume can't cover the whole scene");
        }

        auto prevCascadeCount = volume->cascadeCount;
        ImGui::SliderInt("Cascades count", &volume->cascadeCount, 1, MAX_IRRADIANCE_VOLUME_CASCADES);

        const char* gridResItems [] = { "11x11x11", "15x15x15", "21x21x21", "31x31x31" };
        int currentItem = 0;
        if (volume->probeCount == glm::ivec3(11)) currentItem = 0;
        if (volume->probeCount == glm::ivec3(15)) currentItem = 1;
        if (volume->probeCount == glm::ivec3(21)) currentItem = 2;
        if (volume->probeCount == glm::ivec3(31)) currentItem = 3;
        auto prevItem = currentItem;
        ImGui::Combo("Resolution", &currentItem, gridResItems, IM_ARRAYSIZE(gridResItems));

        if (currentItem != prevItem || prevCascadeCount != volume->cascadeCount) {
            switch (currentItem) {
                case 0: volume->SetProbeCount(glm::ivec3(11), volume->cascadeCount); break;
                case 1: volume->SetProbeCount(glm::ivec3(15), volume->cascadeCount); break;
                case 2: volume->SetProbeCount(glm::ivec3(21), volume->cascadeCount); break;
                case 3: volume->SetProbeCount(glm::ivec3(31), volume->cascadeCount); break;
            }
        }

        const char* rayCountItems[] = { "32", "64", "96", "128", "256", "512" };
        currentItem = 0;
        if (volume->rayCount == 32) currentItem = 0;
        if (volume->rayCount == 64) currentItem = 1;
        if (volume->rayCount == 96) currentItem = 2;
        if (volume->rayCount == 128) currentItem = 3;
        if (volume->rayCount == 256) currentItem = 4;
        if (volume->rayCount == 512) currentItem = 5;
        prevItem = currentItem;
        ImGui::Combo("Ray count", &currentItem, rayCountItems, IM_ARRAYSIZE(rayCountItems));

        if (currentItem != prevItem) {
            switch (currentItem) {
                case 0: volume->SetRayCount(32, 32); break;
                case 1: volume->SetRayCount(64, 32); break;
                case 2: volume->SetRayCount(96, 32); break;
                case 3: volume->SetRayCount(128, 32); break;
                case 4: volume->SetRayCount(256, 32); break;
                case 5: volume->SetRayCount(512, 32); break;
            }
        }

        ImGui::SliderFloat("Strength", &volume->strength, 0.0f, 5.0f);
        ImGui::Separator();

        if (volume->scroll) {
            auto volumeSize = volume->aabb.GetSize();
            ImGui::DragFloat3("Size", (float*)&volumeSize, 0.5f, 1.0f, 2000.0f);
            volume->aabb = volume->aabb.Scale(volumeSize / volume->aabb.GetSize());
        }
        else {
            ImGui::Text("AABB");
            ImGui::DragFloat3("Min", (float*)&volume->aabb.min, 0.5f, -2000.0f, 2000.0f);
            ImGui::DragFloat3("Max", (float*)&volume->aabb.max, 0.5f, -2000.0f, 2000.0f);

            if (ImGui::Button("Adjust to scene size")) {
                volume->aabb = CalculateSceneSize(scene);
            }
        }
        volume->SetAABB(volume->aabb);
       
        ImGui::Separator();
        ImGui::SliderFloat("Hysteresis", &volume->hysteresis, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Sharpness", &volume->sharpness, 0.01f, 200.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Bias", &volume->bias, 0.0f, 1.0f);
        auto prevGamma = volume->gamma;
        ImGui::SliderFloat("Gamma exponent", &volume->gamma, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        if (prevGamma != volume->gamma) volume->ClearProbes();
        ImGui::SliderFloat("Split correction", &volume->splitCorrection, 1.0f, 10.0f);
        ImGui::Separator();
        if (ImGui::Button("Reset probe offsets")) {
            volume->ResetProbeOffsets();
        }
        ImGui::Checkbox("Optimize probes", &volume->optimizeProbes);

        ImGui::PopID();

    }

    Volume::AABB IrradianceVolumePanel::CalculateSceneSize(Ref<Scene::Scene>& scene) const {

        auto min = glm::vec3(std::numeric_limits<float>::max());
        auto max = glm::vec3(-std::numeric_limits<float>::max());

        Volume::AABB aabb(min, max);
        auto meshSubset = scene->GetSubset<MeshComponent>();
        for (auto entity : meshSubset) {
            const auto& meshComponent = meshSubset.Get(entity);

            aabb.Grow(meshComponent.aabb);
        }

        return aabb;

    }

}