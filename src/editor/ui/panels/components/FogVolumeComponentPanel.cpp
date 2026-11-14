#include "FogVolumeComponentPanel.h"
#include "../../Widgets.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool FogVolumeComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, FogVolumeComponent& fogVolumeComponent) {

        if (Widgets::BeginProperties("Fog volume")) {
            Widgets::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &fogVolumeComponent.enable);

            const char* shapes[] = { "Box", "Ellipsoid" };
            auto shape = static_cast<int32_t>(fogVolumeComponent.shape);
            Widgets::PropertyLabel("Shape");
            if (ImGui::Combo("##Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
                fogVolumeComponent.shape = static_cast<FogVolumeShape>(shape);

            Widgets::PropertyLabel("Density");
            ImGui::DragFloat("##Density", &fogVolumeComponent.density, 0.001f, 0.0f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
            Widgets::PropertyLabel("Scattering color");
            ImGui::ColorEdit3("##Scattering color", &fogVolumeComponent.scatteringColor[0]);
            Widgets::PropertyLabel("Scattering factor", "Scales the light the volume scatters towards the camera, "
                "the density alone decides how much it hides");
            ImGui::DragFloat("##Scattering factor", &fogVolumeComponent.scatteringFactor, 0.005f, 0.0f, 16.0f);
            Widgets::PropertyLabel("Scattering anisotropy", "Negative values scatter towards the light: bright halos around "
                "the sun and light shafts. Positive values scatter back to it, 0 evenly in all directions. Same as the "
                "anisotropy of the scene fog");
            ImGui::SliderFloat("##Scattering anisotropy", &fogVolumeComponent.scatteringAnisotropy, -0.99f, 0.99f, "%.2f");
            Widgets::PropertyLabel("Emissive color");
            ImGui::ColorEdit3("##Emissive color", &fogVolumeComponent.emissiveColor[0], ImGuiColorEditFlags_HDR);
            Widgets::PropertyLabel("Ambient factor");
            ImGui::DragFloat("##Ambient factor", &fogVolumeComponent.ambientFactor, 0.005f, 0.0f, 10.0f);

            Widgets::PropertyLabel("Self shadowing");
            ImGui::DragFloat("##Self shadowing", &fogVolumeComponent.selfShadowing, 0.005f, 0.0f, 4.0f);

            Widgets::PropertyLabel("Edge fade");
            ImGui::DragFloat("##Edge fade", &fogVolumeComponent.edgeFade, 0.005f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Height falloff");
            ImGui::DragFloat("##Height falloff", &fogVolumeComponent.heightFalloff, 0.01f, 0.0f, 20.0f);

            Widgets::PropertyLabel("Fade distance", "Zero doesn't fade the volume with distance");
            ImGui::DragFloat("##Fade distance", &fogVolumeComponent.fadeDistance, 1.0f, 0.0f, 10000.0f);

            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Noise");
        if (Widgets::BeginProperties("Fog volume noise")) {
            Widgets::PropertyLabel("Amount");
            ImGui::DragFloat("##Amount", &fogVolumeComponent.noiseAmount, 0.005f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Scale");
            ImGui::DragFloat("##Scale", &fogVolumeComponent.noiseScale, 0.05f, 0.1f, 1000.0f);
            Widgets::PropertyLabel("Coverage");
            ImGui::DragFloat("##Coverage", &fogVolumeComponent.noiseCoverage, 0.005f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Sharpness");
            ImGui::DragFloat("##Sharpness", &fogVolumeComponent.noiseSharpness, 0.005f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Octaves");
            ImGui::SliderInt("##Octaves", &fogVolumeComponent.noiseOctaves, 1, 5);
            Widgets::PropertyLabel("Wind influence");
            ImGui::DragFloat("##Wind influence", &fogVolumeComponent.windInfluence, 0.005f, 0.0f, 2.0f);
            Widgets::EndProperties();
        }

        return false;

    }

}
