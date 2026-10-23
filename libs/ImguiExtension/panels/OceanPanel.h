#pragma once

#include "Panel.h"

#include "ocean/Ocean.h"

namespace Atlas::ImguiExtension {

    class OceanPanel : public Panel {

    public:
        OceanPanel() : Panel("Ocean properties") {}

        void Render(Ref<Ocean::Ocean>& ocean);

    };

}
