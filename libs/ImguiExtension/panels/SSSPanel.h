#pragma once

#include "Panel.h"

#include "lighting/SSS.h"

namespace Atlas::ImguiExtension {

    class SSSPanel : public Panel {

    public:
        SSSPanel() : Panel("SSS properties") {}

        void Render(Ref<Lighting::SSS>& sss);

    };

}