#pragma once

#include "Panel.h"

#include "postprocessing/PostProcessing.h"

namespace Atlas::ImguiExtension {

    class PostProcessingPanel : public Panel {

    public:
        PostProcessingPanel() : Panel("Post process properties") {}

        void Render(PostProcessing::PostProcessing& postProcessing);

    };

}