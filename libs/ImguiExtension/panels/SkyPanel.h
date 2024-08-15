#pragma once

#include "Panel.h"

#include "lighting/Sky.h"
#include "renderer/target/RenderTarget.h"

namespace Atlas::ImguiExtension {

    class SkyPanel : public Panel {

        using CubemapSelector = std::optional<std::function<ResourceHandle<Texture::Cubemap>(ResourceHandle<Texture::Cubemap>)>>;

    public:
        SkyPanel() : Panel("Sky properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, Lighting::Sky& sky, CubemapSelector cubemapSelector = {});

    };

}