#pragma once

#include "Panel.h"

#include "lighting/SSGI.h"

namespace Atlas::ImguiExtension {

    class SSGIPanel : public Panel {

    public:
        SSGIPanel() : Panel("SSGI properties") {}

        void Render(Ref<Lighting::SSGI>& ssgi);

    };

}