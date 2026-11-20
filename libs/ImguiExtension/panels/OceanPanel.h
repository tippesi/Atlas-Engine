#pragma once

#include "Panel.h"

#include "ocean/Ocean.h"

#include <functional>
#include <optional>

namespace Atlas::ImguiExtension {

    class OceanPanel : public Panel {

    public:
        OceanPanel() : Panel("Ocean properties") {}

        using TextureSelector = std::optional<std::function<ResourceHandle<Texture::Texture2D>(ResourceHandle<Texture::Texture2D>)>>;

        void Render(Ref<Ocean::Ocean>& ocean, TextureSelector textureSelector = {});

    };

}
