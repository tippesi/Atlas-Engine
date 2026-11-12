#include "IrradianceVolumePanel.h"
#include "StringHelper.h"

#include <scene/Scene.h>
#include <graphics/GraphicsDevice.h>

namespace Atlas::ImguiExtension {

    void IrradianceVolumePanel::Render(Ref<Lighting::IrradianceVolume> &volume, Ref<Scene::Scene>& scene) {

        ImGui::PushID(GetNameID());

        // Read-only information
        if (UIElements::BeginProperties("IrradianceVolumeInfo")) {
            UIElements::PropertyLabel("Probe count");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(VecToString(volume->probeCount).c_str());
            UIElements::PropertyLabel("Cell size");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(VecToString(volume->cascades[0].cellSize).c_str());
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("General");
        if (UIElements::BeginProperties("IrradianceVolumeGeneral")) {
            UIElements::PropertyLabel("Enable volume");
            ImGui::Checkbox("##Enable volume", &volume->enable);
            UIElements::PropertyLabel("Update volume");
            ImGui::Checkbox("##Update volume", &volume->update);
            UIElements::PropertyLabel("Visualize probes");
            ImGui::Checkbox("##Visualize probes", &volume->debug);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Sampling");
        if (UIElements::BeginProperties("IrradianceVolumeSampling")) {
            UIElements::PropertyLabel("Sample emissives");
            ImGui::Checkbox("##Sample emissives", &volume->sampleEmissives);
            UIElements::PropertyLabel("Calculate visibility");
            ImGui::Checkbox("##Calculate visibility", &volume->visibility);
            UIElements::PropertyLabel("Calculate radiance");
            ImGui::Checkbox("##Calculate radiance", &volume->radiance);

            bool prevMomentRes = volume->lowerResMoments;
            UIElements::PropertyLabel("Lower resolution visibility");
            ImGui::Checkbox("##Lower resolution visibility", &volume->lowerResMoments);
            if (prevMomentRes != volume->lowerResMoments) {
                volume->SetMomentsResolution(volume->lowerResMoments);
            }

            bool prevRadianceRes = volume->lowerResRadiance;
            UIElements::PropertyLabel("Lower resolution radiance");
            ImGui::Checkbox("##Lower resolution radiance", &volume->lowerResRadiance);
            if (prevRadianceRes != volume->lowerResRadiance) {
                volume->SetRadianceResolution(volume->lowerResRadiance);
            }

            UIElements::PropertyLabel("Opacity check");
            ImGui::Checkbox("##Opacity check", &volume->opacityCheck);
            auto reorderSupported = Graphics::GraphicsDevice::DefaultDevice->support.shaderExecutionReordering;
            UIElements::PropertyLabel("Shader execution reordering");
            ImGui::BeginDisabled(!reorderSupported);
            ImGui::Checkbox("##Shader execution reordering", &volume->shaderExecutionReordering);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Sorts the rays by the mesh they hit before shading. Helps with many different meshes, "
                    "costs with few. Needs hit objects, which the device %s.", reorderSupported ? "supports" : "doesn't support");
            }
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Probe grid");
        if (UIElements::BeginProperties("IrradianceVolumeProbeGrid")) {
            auto prevCascadeCount = volume->cascadeCount;
            UIElements::PropertyLabel("Cascades count");
            ImGui::SliderInt("##Cascades count", &volume->cascadeCount, 1, MAX_IRRADIANCE_VOLUME_CASCADES);

            const char* gridResItems [] = { "11x11x11", "15x15x15", "21x21x21", "31x31x31" };
            int currentItem = 0;
            if (volume->probeCount == ivec3(11)) currentItem = 0;
            if (volume->probeCount == ivec3(15)) currentItem = 1;
            if (volume->probeCount == ivec3(21)) currentItem = 2;
            if (volume->probeCount == ivec3(31)) currentItem = 3;
            auto prevItem = currentItem;
            UIElements::PropertyLabel("Resolution");
            ImGui::Combo("##Resolution", &currentItem, gridResItems, IM_ARRAYSIZE(gridResItems));

            if (currentItem != prevItem || prevCascadeCount != volume->cascadeCount) {
                switch (currentItem) {
                    case 0: volume->SetProbeCount(ivec3(11), volume->cascadeCount); break;
                    case 1: volume->SetProbeCount(ivec3(15), volume->cascadeCount); break;
                    case 2: volume->SetProbeCount(ivec3(21), volume->cascadeCount); break;
                    case 3: volume->SetProbeCount(ivec3(31), volume->cascadeCount); break;
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
            UIElements::PropertyLabel("Ray count");
            ImGui::Combo("##Ray count", &currentItem, rayCountItems, IM_ARRAYSIZE(rayCountItems));

            if (currentItem != prevItem) {
                switch (currentItem) {
                    case 0: volume->SetRayCount(32, 16); break;
                    case 1: volume->SetRayCount(64, 16); break;
                    case 2: volume->SetRayCount(96, 16); break;
                    case 3: volume->SetRayCount(128, 16); break;
                    case 4: volume->SetRayCount(256, 16); break;
                    case 5: volume->SetRayCount(512, 16); break;
                }
            }

            UIElements::PropertyLabel("Strength");
            ImGui::SliderFloat("##Strength", &volume->strength, 0.0f, 5.0f);
            UIElements::PropertyLabel("Probe storage scale");
            if (ImGui::SliderFloat("##Probe storage scale", &volume->storageScale, 0.000001f, 1.0f,
                "%.6f", ImGuiSliderFlags_Logarithmic)) {
                volume->ClearProbes();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Smaller values increase lighting range at the cost of precision.\n"
                    "0.1 gives 10x range; lighting brightness is restored when sampled.\n"
                    "Changing this clears probe history.");
            }
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Volume bounds");
        if (UIElements::BeginProperties("IrradianceVolumeBounds")) {
            UIElements::PropertyLabel("Scrolling");
            ImGui::Checkbox("##Scrolling", &volume->scroll);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Scrolls the volume based on a set position (by default the camera). "
                    "This should be used for large scenes where the volume can't cover the whole scene");
            }

            if (volume->scroll) {
                auto volumeSize = volume->aabb.GetSize();
                UIElements::PropertyLabel("Size");
                ImGui::DragFloat3("##Size", (float*)&volumeSize, 0.5f, 1.0f, 2000.0f);
                volume->aabb = volume->aabb.Scale(volumeSize / volume->aabb.GetSize());
            }
            else {
                UIElements::PropertyLabel("Min");
                ImGui::DragFloat3("##Min", (float*)&volume->aabb.min, 0.5f, -2000.0f, 2000.0f);
                UIElements::PropertyLabel("Max");
                ImGui::DragFloat3("##Max", (float*)&volume->aabb.max, 0.5f, -2000.0f, 2000.0f);
            }
            UIElements::EndProperties();
        }

        if (!volume->scroll) {
            if (ImGui::Button("Adjust to scene size", ImVec2(-FLT_MIN, 0.0f))) {
                volume->aabb = CalculateSceneSize(scene);
            }
        }
        volume->SetAABB(volume->aabb);

        ImGui::SeparatorText("Filtering");
        if (UIElements::BeginProperties("IrradianceVolumeFiltering")) {
            UIElements::PropertyLabel("Hysteresis");
            ImGui::SliderFloat("##Hysteresis", &volume->hysteresis, 0.0f, 1.0f, "%.3f");
            UIElements::PropertyLabel("Sharpness");
            ImGui::SliderFloat("##Sharpness", &volume->sharpness, 0.01f, 200.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            UIElements::PropertyLabel("Bias");
            ImGui::SliderFloat("##Bias", &volume->bias, 0.0f, 1.0f);
            auto prevGamma = volume->gamma;
            UIElements::PropertyLabel("Gamma exponent");
            ImGui::SliderFloat("##Gamma exponent", &volume->gamma, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            if (prevGamma != volume->gamma) volume->ClearProbes();
            UIElements::PropertyLabel("Split correction");
            ImGui::DragFloat("##Split correction", &volume->splitCorrection, 0.01f, 1.0f, 10.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Probe optimization");
        if (UIElements::BeginProperties("IrradianceVolumeProbeOptimization")) {
            UIElements::PropertyLabel("Optimize probes");
            ImGui::Checkbox("##Optimize probes", &volume->optimizeProbes);
            UIElements::EndProperties();
        }
        if (ImGui::Button("Reset probe offsets", ImVec2(-FLT_MIN, 0.0f))) {
            volume->ResetProbeOffsets();
        }

        ImGui::PopID();

    }

    Volume::AABB IrradianceVolumePanel::CalculateSceneSize(Ref<Scene::Scene>& scene) const {

        auto min = vec3(std::numeric_limits<float>::max());
        auto max = vec3(-std::numeric_limits<float>::max());

        Volume::AABB aabb(min, max);
        auto meshSubset = scene->GetSubset<MeshComponent>();
        for (auto entity : meshSubset) {
            const auto& meshComponent = meshSubset.Get(entity);

            aabb.Grow(meshComponent.aabb);
        }

        return aabb;

    }

}
