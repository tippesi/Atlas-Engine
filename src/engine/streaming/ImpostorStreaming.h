#pragma once

#include "../System.h"
#include "Residency.h"
#include "StreamingSettings.h"
#include "../mesh/Impostor.h"

#include <span>

namespace Atlas::Streaming {

    /**
     * Keeps the views of streamed impostors at the resolution their instances need on screen, within the impostor
     * budget. Impostors which aren't requested leave the GPU. The impostor files stay in RAM, changes are uploaded in
     * jobs and swap the textures of the impostor, everything keeps referencing the same impostor object.
     */
    class ImpostorStreaming {

    public:
        struct ImpostorRequest {
            const Mesh::Impostor* impostor;
            // Texels on a side of a view
            float resolution;
        };

        // Loads an impostor which is streamed, starting at the minimum resolution. Meant to be used as the loader of resources
        static Ref<Mesh::Impostor> LoadImpostor(const std::string& filename, bool binaryJson);

        // Requests the resolutions until the next update, the largest request counts
        static void Request(std::span<const ImpostorRequest> requests);

        /**
         * Texels on a side of a view for one texel per pixel.
         * @param radius The radius of the impostor
         * @param distance The distance to the center of the impostor in the same unit
         * @param focalLength The viewport height in pixels divided by 2 * tan(fov / 2)
         */
        static float GetResolution(float radius, float distance, float focalLength);

        static Residency residency;

    private:
        class Entry;

    };

}
