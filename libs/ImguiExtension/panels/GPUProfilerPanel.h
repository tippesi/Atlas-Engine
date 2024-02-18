#pragma once

#include "Panel.h"

#include "lighting/IrradianceVolume.h"

namespace Atlas::ImguiExtension {

    class GPUProfilerPanel : public Panel {

    public:
        GPUProfilerPanel() : Panel("GPU profiler hierarchy") {}

        void Render();

    };

}