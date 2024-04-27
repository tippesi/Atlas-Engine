#pragma once

#include "Panel.h"

#include "lighting/Fog.h"
#include "renderer/target/RenderTarget.h"

namespace Atlas::ImguiExtension {

    class FogPanel : public Panel {

    public:
        FogPanel() : Panel("Fog properties") {}

        void Render(Ref<Lighting::Fog>& fog, Ref<Renderer::RenderTarget>& target);

    };

}