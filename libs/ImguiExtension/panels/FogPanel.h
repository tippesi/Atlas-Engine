#pragma once

#include "Panel.h"

#include "lighting/Fog.h"

namespace Atlas::ImguiExtension {

    class FogPanel : public Panel {

    public:
        FogPanel() : Panel("Fog properties") {}

        void Render(Ref<Lighting::Fog>& fog);

    };

}