#pragma once

#include "../System.h"
#include "../streaming/StreamingSettings.h"
#include "../postprocessing/PostProcessing.h"

namespace Atlas {

    // The technical settings of a project, the same for all of its scenes
    struct ProjectSettings {

        // Defaults of the streaming limits, a game can change them in its graphics menu
        Streaming::StreamingSettings streaming;

        bool occlusionCulling = true;
        // A game can let the player choose another one in its graphics menu
        PostProcessing::Upscaler upscaler = PostProcessing::Upscaler::FSR2;
        float meshLodErrorScale = 1.0f;
        // Scenes are saved with columns of raw values, they load about twice as fast
        bool binarySceneColumns = false;

    };

}
