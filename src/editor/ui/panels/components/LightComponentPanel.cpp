#include "LightComponentPanel.h"

#include <imgui.h>

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool LightComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, LightComponent &lightComponent) {

        const char* typeItems[] = { "Directional" };
        int currentItem = static_cast<int>(lightComponent.type);
        ImGui::Combo("Light type", &currentItem, typeItems, IM_ARRAYSIZE(typeItems));
        lightComponent.type = static_cast<LightType>(currentItem);

        bool isStatic = lightComponent.mobility == LightMobility::StationaryLight;
        ImGui::Checkbox("Static", &isStatic);
        lightComponent.mobility = isStatic ? LightMobility::StationaryLight : LightMobility::MovableLight;

        ImGui::Separator();

        ImGui::Text("Type properties");

        if (lightComponent.type == LightType::DirectionalLight) {
            ImGui::Checkbox("Main", &lightComponent.isMain);
            auto& directional = lightComponent.properties.directional;
            ImGui::DragFloat3("Direction", &directional.direction[0], 0.005f, -1.0f, 1.0f);
        }
        else if (lightComponent.type == LightType::PointLight) {

        }

        ImGui::Separator();

        ImGui::Text("General properties");

        bool castShadow = lightComponent.shadow != nullptr;
        ImGui::Checkbox("Shadow", &castShadow);
        ImGui::ColorEdit3("Color", &lightComponent.color[0]);
        ImGui::DragFloat("Intensity", &lightComponent.intensity, 0.1f, 0.0f, 1000.0f);

        if (!lightComponent.shadow && castShadow) {
            //lightComponent.AddDirectionalShadow(300.0f, 3.0f, 1024, 3, 0.95f, true, 2048.0f);
            if (lightComponent.type == LightType::DirectionalLight) {
                lightComponent.AddDirectionalShadow(300.0f, 3.0f, 1024, 3, 0.95f, true, 2048.0f);
            }
        }
        else if (lightComponent.shadow && !castShadow) {
            lightComponent.AddDirectionalShadow(300.0f, 3.0f, 1024, 3, 0.95f, true, 2048.0f);
        }

        if (castShadow) {

            ImGui::Separator();

            ImGui::Text("Shadow");

            ImGui::SliderFloat("Bias##Shadow", &lightComponent.shadow->bias, 0.0f, 3.0f);

        }



        return false;

    }

}