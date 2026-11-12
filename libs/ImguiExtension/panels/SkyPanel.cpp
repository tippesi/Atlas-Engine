#include "SkyPanel.h"

namespace Atlas::ImguiExtension {

    void SkyPanel::Render(Ref<ImguiWrapper>& wrapper, Lighting::Sky& sky, CubemapSelector cubemapSelector) {

        ImGui::PushID(GetNameID());

        ResourceHandle<Texture::Cubemap> cubemap;
        if (sky.probe)
            cubemap = sky.probe->cubemap;

        ImGui::SeparatorText("Environment map");
        if (cubemapSelector.has_value()) {
            cubemap = cubemapSelector.value()(cubemap);
        }

        if (cubemap.IsValid() && !sky.probe)
            sky.probe = CreateRef<Lighting::EnvironmentProbe>(cubemap);
        else if (!cubemap.IsValid() && sky.probe)
            sky.probe = nullptr;

        if (sky.atmosphere) {
            auto atmosphere = sky.atmosphere;

            atmosphere->rayleighScatteringCoeff *= 1e6f;
            atmosphere->mieScatteringCoeff *= 1e6f;

            ImGui::SeparatorText("Atmosphere");
            if (UIElements::BeginProperties("SkyAtmosphere")) {
                bool atmosphereChanged = false;
                UIElements::PropertyLabel("Rayleigh scattering coefficient");
                atmosphereChanged |= ImGui::DragFloat3("##Rayleigh scattering coefficient", glm::value_ptr(atmosphere->rayleighScatteringCoeff), 0.1f, 0.0f, 10e10f);
                UIElements::PropertyLabel("Ground albedo");
                atmosphereChanged |= ImGui::ColorEdit3("##Ground albedo", glm::value_ptr(atmosphere->groundAlbedo));
                UIElements::PropertyLabel("Mie scattering coefficient");
                atmosphereChanged |= ImGui::DragFloat("##Mie scattering coefficient", &atmosphere->mieScatteringCoeff, 1.0f, 0.0f, 10e10f);
                UIElements::PropertyLabel("Rayleigh height scale");
                atmosphereChanged |= ImGui::DragFloat("##Rayleigh height scale", &atmosphere->rayleighHeightScale, 100.0f, 0.0f, 10e10f);
                UIElements::PropertyLabel("Mie height scale");
                atmosphereChanged |= ImGui::DragFloat("##Mie height scale", &atmosphere->mieHeightScale, 100.0f, 0.0f, 10e10f);
                atmosphere->needsUpdate |= atmosphereChanged;
                UIElements::PropertyLabel("Sun disk size", "Angular diameter of the sun, the real sun has about 0.53 degrees");
                ImGui::DragFloat("##Sun disk size", &atmosphere->sunDiskSize, 0.01f, 0.01f, 20.0f, "%.2f deg");
                UIElements::PropertyLabel("Direct light", "How much the air colors and dims the sun's light, red and weak near the horizon at 1");
                ImGui::SliderFloat("##Direct light", &atmosphere->directLight, 0.0f, 1.0f);
                UIElements::PropertyLabel("Indirect light", "How much the sky lights the scene: ambient light, global illumination and sky reflections");
                ImGui::SliderFloat("##Indirect light", &atmosphere->indirectLight, 0.0f, 2.0f);
                UIElements::EndProperties();
            }

            atmosphere->rayleighScatteringCoeff /= 1e6f;
            atmosphere->mieScatteringCoeff /= 1e6f;
        }

        ImGui::PopID();

    }

}
