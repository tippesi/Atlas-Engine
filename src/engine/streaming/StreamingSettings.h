#pragma once

#include "../System.h"

namespace Atlas::Streaming {

    // Limits of the streaming, meant to be changed by the user (e.g. in a graphics menu)
    struct StreamingSettings {

        struct Limits {
            bool enable = true;
            // In megabytes, what is above it is lowered evenly or, for meshes, shown as impostors only
            int32_t budget = 2048;
            // In updates, a lower request has to persist this long
            int32_t reductionDelay = 120;
            int32_t maxPendingLoads = 8;
        };

        // Resources which are streamed by their mip levels
        struct Levels : Limits {
            // They don't go below this resolution and load with it first
            int32_t minResolution = 8;
            float resolutionScale = 1.0f;
        };

        struct Meshes : Limits {
            // Meshes and their impostors are loaded once their closest instance is this close to where they are needed
            float distanceMargin = 50.0f;
            // Meshes keep their geometry for ray tracing up to this distance at most
            float rayTracingDistance = 500.0f;
        };

        Levels textures;

        // The resolution of the views of the impostors, impostors of meshes without a needed instance leave the GPU
        Levels impostors = { {}, 16 };

        Meshes meshes = { { .reductionDelay = 300, .maxPendingLoads = 4 } };

    };

}
