#pragma once

#include "Panel.h"

#include "Material.h"

#include <optional>
#include <functional>

namespace Atlas::ImguiExtension {

    class MaterialPanel : public Panel {

    public:
        using TextureSelector = std::optional<std::function<ResourceHandle<Texture::Texture2D>(ResourceHandle<Texture::Texture2D>)>>;

    public:
        MaterialPanel() : Panel("Material properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, Ref<Material>& material, TextureSelector textureSelector = {});

    };

}