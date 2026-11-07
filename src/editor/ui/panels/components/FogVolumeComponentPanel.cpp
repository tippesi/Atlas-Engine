#include "FogVolumeComponentPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool FogVolumeComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, FogVolumeComponent& fogVolumeComponent) {

        ImGui::Checkbox("Enable", &fogVolumeComponent.enable);

        const char* shapes[] = { "Box", "Ellipsoid" };
        auto shape = static_cast<int32_t>(fogVolumeComponent.shape);
        if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
            fogVolumeComponent.shape = static_cast<FogVolumeShape>(shape);

        ImGui::DragFloat("Density", &fogVolumeComponent.density, 0.001f, 0.0f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::ColorEdit3("Scattering color", &fogVolumeComponent.scatteringColor[0]);
        ImGui::ColorEdit3("Emissive color", &fogVolumeComponent.emissiveColor[0], ImGuiColorEditFlags_HDR);
        ImGui::DragFloat("Ambient factor", &fogVolumeComponent.ambientFactor, 0.005f, 0.0f, 10.0f);

        ImGui::DragFloat("Self shadowing", &fogVolumeComponent.selfShadowing, 0.005f, 0.0f, 4.0f);

        ImGui::DragFloat("Edge fade", &fogVolumeComponent.edgeFade, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Height falloff", &fogVolumeComponent.heightFalloff, 0.01f, 0.0f, 20.0f);

        ImGui::DragFloat("Fade distance (0 = none)", &fogVolumeComponent.fadeDistance, 1.0f, 0.0f, 10000.0f);

        ImGui::SeparatorText("Noise");
        ImGui::DragFloat("Amount", &fogVolumeComponent.noiseAmount, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Scale", &fogVolumeComponent.noiseScale, 0.05f, 0.1f, 1000.0f);
        ImGui::DragFloat("Coverage", &fogVolumeComponent.noiseCoverage, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Sharpness", &fogVolumeComponent.noiseSharpness, 0.005f, 0.0f, 1.0f);
        ImGui::SliderInt("Octaves", &fogVolumeComponent.noiseOctaves, 1, 5);
        ImGui::DragFloat("Wind influence", &fogVolumeComponent.windInfluence, 0.005f, 0.0f, 2.0f);

        return false;

    }

}
