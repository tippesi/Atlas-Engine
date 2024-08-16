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

        ImGui::PopID();

    }

}