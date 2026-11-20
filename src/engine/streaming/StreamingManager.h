#pragma once

#include "../System.h"
#include "StreamingSettings.h"
#include "TextureStreaming.h"
#include "ImpostorStreaming.h"
#include "MeshStreaming.h"

namespace Atlas::Streaming {

    /**
     * Keeps the GPU side of the resources at the detail their requesters need, within the limits of the settings. The
     * resources themselves stay loaded with their data in RAM, the resource manager decides about that.
     * Requesters ask every frame: the scenes for the closest instances of their meshes, other systems for what they are
     * about to show (e.g. world cells for the resources of the cells they load). Uploads run in jobs, their results are
     * swapped in during the update, so everything keeps referencing the same resource objects.
     */
    class StreamingManager {

    public:
        struct Stats {
            ResidencyStats textures;
            ResidencyStats impostors;
            ResidencyStats meshes;
        };

        // Swaps in finished loads and starts new ones, has to run while nothing renders. Calls in the same frame are ignored
        static void Update();

        static Stats GetStats();

        static void Shutdown();

        static StreamingSettings settings;

    private:
        static int32_t lastUpdateFrame;

    };

}
