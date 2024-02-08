#pragma once

#include "Panel.h"

#include "lighting/VolumetricClouds.h"

namespace Atlas::ImguiExtension {

    class VolumetricCloudsPanel : public Panel {

    public:
        VolumetricCloudsPanel() : Panel("Volumetric cloud properties") {}

        void Render(Ref<Lighting::VolumetricClouds>& clouds);

        bool debug = false;

    };

}