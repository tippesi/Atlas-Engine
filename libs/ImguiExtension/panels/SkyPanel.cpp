#include "SkyPanel.h"

namespace Atlas::ImguiExtension {

    void SkyPanel::Render(Ref<ImguiWrapper>& wrapper, Lighting::Sky& sky, CubemapSelector cubemapSelector) {

        ImGui::PushID(GetNameID());

        ResourceHandle<Texture::Cubemap> cubemap;
        if (sky.probe)
            cubemap = sky.probe->cubemap;

        ImGui::Text("Environment map");
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

            ImGui::Separator();
            ImGui::Text("Atmosphere");
            ImGui::DragFloat3("Rayleigh scattering coefficient", glm::value_ptr(atmosphere->rayleighScatteringCoeff), 0.1f, 0.0f, 10e10f);
            ImGui::DragFloat("Mie scattering coefficient", &atmosphere->mieScatteringCoeff, 1.0f, 0.0f, 10e10f);
            ImGui::DragFloat("Rayleigh height scale", &atmosphere->rayleighHeightScale, 100.0f, 0.0f, 10e10f);
            ImGui::DragFloat("Mie height scale", &atmosphere->mieHeightScale, 100.0f, 0.0f, 10e10f);

            atmosphere->rayleighScatteringCoeff /= 1e6f;
            atmosphere->mieScatteringCoeff /= 1e6f;
        }

        ImGui::PopID();

    }

}