#pragma once

#include "Panel.h"

#include "lighting/VolumetricClouds.h"
#include "renderer/target/RenderTarget.h"

namespace Atlas::ImguiExtension {

    class VolumetricCloudsPanel : public Panel {

    public:
        VolumetricCloudsPanel() : Panel("Volumetric cloud properties") {}

        void Render(Ref<Lighting::VolumetricClouds>& clouds, Ref<Renderer::RenderTarget>& target);

        bool debug = false;

    };

}