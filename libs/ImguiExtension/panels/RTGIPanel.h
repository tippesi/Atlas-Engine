#pragma once

#include "Panel.h"

#include "lighting/RTGI.h"
#include "renderer/target/RenderTarget.h"

namespace Atlas::ImguiExtension {

    class RTGIPanel : public Panel {

    public:
        RTGIPanel() : Panel("RTGI properties") {}

        void Render(Ref<Lighting::RTGI>& rtgi, Ref<Renderer::RenderTarget>& target);

    };

}