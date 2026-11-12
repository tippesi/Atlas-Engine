#include "LightComponentPanel.h"
#include "../../Widgets.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool LightComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, LightComponent &lightComponent) {

        if (Widgets::BeginProperties("LightComponent")) {
            const char* typeItems[] = { "Directional", "Point", "Spot"};
            int typeItem = static_cast<int>(lightComponent.type);
            Widgets::PropertyLabel("Light type");
            ImGui::Combo("##Light type", &typeItem, typeItems, IM_ARRAYSIZE(typeItems));
            lightComponent.type = static_cast<LightType>(typeItem);

            bool isStatic = lightComponent.mobility == LightMobility::StationaryLight;
            Widgets::PropertyLabel("Static");
            ImGui::Checkbox("##Static", &isStatic);
            lightComponent.mobility = isStatic ? LightMobility::StationaryLight : LightMobility::MovableLight;
            Widgets::EndProperties();
        }

        // The properties of the selected type of light
        const char* typeSection = lightComponent.type == LightType::DirectionalLight ? "Directional" :
            lightComponent.type == LightType::PointLight ? "Point" : "Spot";
        ImGui::SeparatorText(typeSection);

        if (Widgets::BeginProperties("LightType")) {
            if (lightComponent.type == LightType::DirectionalLight) {
                Widgets::PropertyLabel("Main");
                ImGui::Checkbox("##Main", &lightComponent.isMain);
                auto& directional = lightComponent.properties.directional;
                Widgets::PropertyLabel("Direction");
                Widgets::DragVec3("##Direction", directional.direction, 0.005f, 0.0f, -1.0f, 1.0f);
            }
            else if (lightComponent.type == LightType::PointLight) {
                auto& point = lightComponent.properties.point;
                Widgets::PropertyLabel("Position");
                Widgets::DragVec3("##Position", point.position, 0.1f, 0.0f, -10000.0f, 10000.0f);
                Widgets::PropertyLabel("Radius");
                ImGui::DragFloat("##Radius", &point.radius, 0.01f, 0.01f, 10000.0f);
            }
            else if (lightComponent.type == LightType::SpotLight) {
                auto& spot = lightComponent.properties.spot;
                Widgets::PropertyLabel("Position");
                Widgets::DragVec3("##Position", spot.position, 0.1f, 0.0f, -10000.0f, 10000.0f);
                Widgets::PropertyLabel("Direction");
                Widgets::DragVec3("##Direction", spot.direction, 0.01f, 0.0f, -1.0f, 1.0f);
                Widgets::PropertyLabel("Radius");
                ImGui::DragFloat("##Radius", &spot.radius, 0.01f, 0.01f, 10000.0f);
                Widgets::PropertyLabel("Inner cone angle");
                ImGui::DragFloat("##Inner cone angle", &spot.innerConeAngle, 0.01f, 0.01f, 2.0f);
                Widgets::PropertyLabel("Outer cone angle");
                ImGui::DragFloat("##Outer cone angle", &spot.outerConeAngle, 0.01f, 0.01f, 2.0f);
            }
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("General");

        bool castShadow = lightComponent.shadow != nullptr;
        if (Widgets::BeginProperties("LightGeneral")) {
            Widgets::PropertyLabel("Shadow");
            ImGui::Checkbox("##Shadow", &castShadow);
            Widgets::PropertyLabel("Color");
            ImGui::ColorEdit3("##Color", &lightComponent.color[0]);
            Widgets::PropertyLabel("Intensity");
            ImGui::DragFloat("##Intensity", &lightComponent.intensity, 0.1f, 0.0f, 1000.0f);
            Widgets::PropertyLabel("Volumetric intensity");
            ImGui::DragFloat("##Volumetric intensity", &lightComponent.volumetricIntensity, 0.1f, 0.0f, 10.0f);
            Widgets::EndProperties();
        }

        if (!lightComponent.shadow && castShadow) {
            if (lightComponent.type == LightType::DirectionalLight) {
                lightComponent.AddDirectionalShadow(300.0f, 3.0f, 1024, 0.05f, 3, 0.95f, true, 2048.0f);
            }
            if (lightComponent.type == LightType::PointLight) {
                lightComponent.AddPointShadow(0.25f, 1024);
            }
            if (lightComponent.type == LightType::SpotLight) {
                lightComponent.AddSpotShadow(2.0f, 1024);
            }
        }
        else if (lightComponent.shadow && !castShadow) {
            lightComponent.shadow = nullptr;
        }

        if (castShadow) {

            auto& shadow = lightComponent.shadow;

            ImGui::SeparatorText("Shadow");

            const char* shadowResItems[] = { "512x512", "1024x1024", "2048x2048", "4096x4096", "8192x8192" };
            int shadowResItem = 0;
            if (shadow->resolution == 512) shadowResItem = 0;
            if (shadow->resolution == 1024) shadowResItem = 1;
            if (shadow->resolution == 2048) shadowResItem = 2;
            if (shadow->resolution == 4096) shadowResItem = 3;
            if (shadow->resolution == 8192) shadowResItem = 4;
            auto prevItem = shadowResItem;

            if (Widgets::BeginProperties("LightShadow")) {
                Widgets::PropertyLabel("Resolution");
                ImGui::Combo("##Resolution", &shadowResItem, shadowResItems, IM_ARRAYSIZE(shadowResItems));
                Widgets::PropertyLabel("Bias");
                ImGui::DragFloat("##Bias", &shadow->bias, 0.05f, 0.0f, 10.0f);
                Widgets::PropertyLabel("Distance");
                ImGui::DragFloat("##Distance", &shadow->distance, 1.0f, 0.0f, 10000.0f);
                Widgets::PropertyLabel("Edge softness");
                ImGui::DragFloat("##Edge softness", &shadow->edgeSoftness, 0.005f, 0.0f, 1.0f);
                Widgets::EndProperties();
            }

            if (shadowResItem != prevItem) {
                switch (shadowResItem) {
                case 0: shadow->SetResolution(512); break;
                case 1: shadow->SetResolution(1024); break;
                case 2: shadow->SetResolution(2048); break;
                case 3: shadow->SetResolution(4096); break;
                case 4: shadow->SetResolution(8192); break;
                }
            }

            if (lightComponent.type == LightType::DirectionalLight) {
                ImGui::SeparatorText("Cascades");

                // The split correction and the long range distance are read every frame, a different number of cascades
                // needs new maps. Long range takes one of the views
                auto cascadeCount = shadow->viewCount - (shadow->longRange ? 1 : 0);
                auto longRange = shadow->longRange;
                if (Widgets::BeginProperties("LightShadowCascaded")) {
                    Widgets::PropertyLabel("Count", "Cascades split the shadow distance, more of them give sharper "
                        "shadows close to the camera");
                    ImGui::SliderInt("##Cascade count", &cascadeCount, 1, MAX_SHADOW_VIEW_COUNT - (longRange ? 1 : 0));
                    Widgets::PropertyLabel("Split correction", "0 splits the distance evenly, 1 gives the close cascades less of it");
                    ImGui::DragFloat("##Split correction", &shadow->splitCorrection, 0.01f, 0.0f, 1.0f);
                    Widgets::PropertyLabel("Long range", "One more cascade with a coarse shadow far beyond the distance");
                    ImGui::Checkbox("##Long range", &longRange);
                    ImGui::BeginDisabled(!longRange);
                    Widgets::PropertyLabel("Long range distance");
                    if (ImGui::DragFloat("##Long range distance", &shadow->longRangeDistance, 10.0f, 10.0f, 10000.0f) &&
                        shadow->longRange)
                        shadow->views.back().farDistance = shadow->longRangeDistance;
                    ImGui::EndDisabled();
                    Widgets::EndProperties();
                }

                cascadeCount = std::min(cascadeCount, MAX_SHADOW_VIEW_COUNT - (longRange ? 1 : 0));
                if (cascadeCount != shadow->viewCount - (shadow->longRange ? 1 : 0) || longRange != shadow->longRange) {
                    auto previous = shadow;
                    lightComponent.AddDirectionalShadow(previous->distance, previous->bias, previous->resolution, previous->edgeSoftness,
                        cascadeCount + (longRange ? 1 : 0), previous->splitCorrection, longRange, previous->longRangeDistance);
                    shadow->longRangeDistance = previous->longRangeDistance;
                    shadow->allowTerrain = previous->allowTerrain;
                    shadow->allowDynamicEntities = previous->allowDynamicEntities;
                    shadow->cascadeBlendDistance = previous->cascadeBlendDistance;
                }
            }
        }

        return false;

    }

}