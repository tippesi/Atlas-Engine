#include "LightComponentPanel.h"

#include <imgui.h>

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool LightComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, LightComponent &lightComponent) {

        const char* typeItems[] = { "Directional", "Point", "Spot"};
        int typeItem = static_cast<int>(lightComponent.type);
        ImGui::Combo("Light type", &typeItem, typeItems, IM_ARRAYSIZE(typeItems));
        lightComponent.type = static_cast<LightType>(typeItem);

        bool isStatic = lightComponent.mobility == LightMobility::StationaryLight;
        ImGui::Checkbox("Static", &isStatic);
        lightComponent.mobility = isStatic ? LightMobility::StationaryLight : LightMobility::MovableLight;

        ImGui::Separator();

        ImGui::Text("Type properties");

        if (lightComponent.type == LightType::DirectionalLight) {
            ImGui::Checkbox("Main", &lightComponent.isMain);
            auto& directional = lightComponent.properties.directional;
            ImGui::DragFloat3("Direction", glm::value_ptr(directional.direction), 0.005f, -1.0f, 1.0f);
        }
        else if (lightComponent.type == LightType::PointLight) {
            auto& point = lightComponent.properties.point;
            ImGui::DragFloat3("Position", glm::value_ptr(point.position), 0.1f, -10000.0f, 10000.0f);
            ImGui::DragFloat("Radius", &point.radius, 0.01f, 0.01f, 10000.0f);
        }
        else if (lightComponent.type == LightType::SpotLight) {
            auto& spot = lightComponent.properties.spot;
            ImGui::DragFloat3("Position", glm::value_ptr(spot.position), 0.1f, -10000.0f, 10000.0f);
            ImGui::DragFloat3("Direction", glm::value_ptr(spot.direction), 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("Radius", &spot.radius, 0.01f, 0.01f, 10000.0f);
            ImGui::DragFloat("Inner cone angle", &spot.innerConeAngle, 0.01f, 0.01f, 2.0f);
            ImGui::DragFloat("Outer cone angle", &spot.outerConeAngle, 0.01f, 0.01f, 2.0f);
        }

        ImGui::Separator();

        ImGui::Text("General properties");

        bool castShadow = lightComponent.shadow != nullptr;
        ImGui::Checkbox("Shadow", &castShadow);
        ImGui::ColorEdit3("Color", &lightComponent.color[0]);
        ImGui::DragFloat("Intensity", &lightComponent.intensity, 0.1f, 0.0f, 1000.0f);
        ImGui::DragFloat("Volumetric intensity", &lightComponent.volumetricIntensity, 0.1f, 0.0f, 10.0f);

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

            ImGui::Separator();

            ImGui::Text("Shadow");

            const char* shadowResItems[] = { "512x512", "1024x1024", "2048x2048", "4096x4096", "8192x8192" };
            int shadowResItem = 0;
            if (shadow->resolution == 512) shadowResItem = 0;
            if (shadow->resolution == 1024) shadowResItem = 1;
            if (shadow->resolution == 2048) shadowResItem = 2;
            if (shadow->resolution == 4096) shadowResItem = 3;
            if (shadow->resolution == 8192) shadowResItem = 4;
            auto prevItem = shadowResItem;
            ImGui::Combo("Resolution", &shadowResItem, shadowResItems, IM_ARRAYSIZE(shadowResItems));

            if (shadowResItem != prevItem) {
                switch (shadowResItem) {
                case 0: shadow->SetResolution(512); break;
                case 1: shadow->SetResolution(1024); break;
                case 2: shadow->SetResolution(2048); break;
                case 3: shadow->SetResolution(4096); break;
                case 4: shadow->SetResolution(8192); break;
                }
            }

            ImGui::DragFloat("Bias", &shadow->bias, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Distance", &shadow->distance, 1.0f, 0.0f, 10000.0f);
            ImGui::DragFloat("Edge softness", &shadow->edgeSoftness, 0.005f, 0.0f, 1.0f);

            ImGui::Separator();

            if (lightComponent.type == LightType::DirectionalLight) {
                ImGui::Checkbox("Is cascaded", &shadow->isCascaded);

                if (!shadow->isCascaded) {
                    ImGui::Checkbox("Follow main camera", &shadow->followMainCamera);

                    if (!shadow->followMainCamera) {
                        ImGui::DragFloat3("Center point", glm::value_ptr(shadow->center), 0.5f, -10000.0f, 10000.0f);
                    }

                    auto orthoSize = shadow->views.front().orthoSize;
                    if (orthoSize.x == 0.0f && orthoSize.y == 0.0f)
                        orthoSize = vec4(-100.0f, 100.0f, -100.0f, 100.0f);

                    ImGui::Text("Width");
                    ImGui::DragFloat("Min##Width", &orthoSize.x, 1.0f, -10000.0f, 10000.0f);
                    ImGui::DragFloat("Max##Width", &orthoSize.y, 1.0f, -10000.0f, 10000.0f);

                    ImGui::Text("Height");
                    ImGui::DragFloat("Min##Height", &orthoSize.z, 1.0f, -10000.0f, 10000.0f);
                    ImGui::DragFloat("Max##Height", &orthoSize.w, 1.0f, -10000.0f, 10000.0f);

                    auto matrix = glm::ortho(-200.0f, 200.0f, -200.0f, 200.0f, -120.0f, 120.0f);
                    lightComponent.AddDirectionalShadow(shadow->distance, shadow->bias, shadow->resolution,
                        shadow->edgeSoftness, shadow->center, orthoSize);
                }
                else {
                    ImGui::SliderInt("Cascade count", &shadow->viewCount, 1, 5);
                    ImGui::DragFloat("Split correction", &shadow->splitCorrection, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox("Long range", &shadow->longRange);
                    ImGui::DragFloat("Long range distance", &shadow->longRangeDistance, 10.0f, 10.0f, 10000.0f);

                    lightComponent.AddDirectionalShadow(shadow->distance, shadow->bias, shadow->resolution, shadow->edgeSoftness,
                        shadow->viewCount, shadow->splitCorrection, shadow->longRange, shadow->longRangeDistance);
                }
            }
            else if (lightComponent.type == LightType::PointLight) {


            }

            

        }

        return false;

    }

}