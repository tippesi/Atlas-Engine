#include "SkyPanel.h"

namespace Atlas::ImguiExtension {

    void SkyPanel::Render(Ref<ImguiWrapper>& wrapper, Lighting::Sky& sky, CubemapSelector cubemapSelector) {

        ImGui::PushID(GetNameID());

        ResourceHandle<Texture::Cubemap> cubemap;
        if (sky.probe)
            cubemap = sky.probe->cubemap;

        UIElements::PropertySection("Environment map");
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

            UIElements::PropertySection("Atmosphere");
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
                const char* resolutions[] = { "32", "64", "128", "256", "512", "1024" };
                auto resolutionIdx = glm::clamp(int32_t(std::log2(float(atmosphere->GetProbeResolution()))) - 5, 0, 5);
                UIElements::PropertyLabel("Probe resolution", "Width of a cubemap face of the sky which lights the scene, sharper sky reflections need more. Reflections use up to 512");
                if (ImGui::Combo("##Probe resolution", &resolutionIdx, resolutions, IM_ARRAYSIZE(resolutions)))
                    atmosphere->SetProbeResolution(32 << resolutionIdx);
                UIElements::EndProperties();
            }

            atmosphere->rayleighScatteringCoeff /= 1e6f;
            atmosphere->mieScatteringCoeff /= 1e6f;
        }

        ImGui::PopID();

    }

}
