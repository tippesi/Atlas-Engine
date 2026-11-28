#pragma once

#include "../System.h"
#include "Residency.h"
#include "StreamingSettings.h"
#include "../texture/Texture2D.h"

#include <span>
#include <vector>

namespace Atlas {

    class Material;

}

namespace Atlas::Streaming {

    /**
     * Keeps streamed textures at the resolution their requesters need, within the texture budget. The ktx2 files stay
     * in RAM, changes are uploaded in jobs and swap the image of the texture, materials and bindless slots keep
     * referencing the same texture object.
     */
    class TextureStreaming {

    public:
        struct MaterialRequest {
            const Material* material;
            // Texels per unit of the texture coordinates
            float resolution;
        };

        /**
         * Loads a texture which is streamed if it is a ktx2 file with mip levels, starting at the minimum resolution.
         * Meant to be used as the loader of resources, e.g. for the maps of materials.
         */
        static Ref<Texture::Texture2D> LoadTexture(const std::string& filename, bool colorSpaceConversion,
            Texture::Wrapping wrapping, Texture::Filtering filtering);

        // Same for a ktx2 file that is already in memory, e.g. when it was just converted
        static Ref<Texture::Texture2D> LoadTextureFromMemory(std::vector<char> data, const std::string& filename,
            bool colorSpaceConversion, Texture::Wrapping wrapping, Texture::Filtering filtering);

        // Requests the resolutions for all maps of the materials until the next update, the largest request counts
        static void Request(std::span<const MaterialRequest> requests);

        /**
         * Texels per unit of the texture coordinates for one texel per pixel.
         * @param uvScale The length covered by one unit of the texture coordinates (e.g. Mesh::MeshSubData::uvScale)
         * @param distance The distance to the surface in the same unit
         * @param focalLength The viewport height in pixels divided by 2 * tan(fov / 2)
         */
        static float GetResolution(float uvScale, float distance, float focalLength);

        // Loads all streamed maps of the material with their full resolution right away (e.g. for baking), has to
        // run while nothing renders. Waits for maps that are still loading, they are reduced again once they aren't requested
        static void LoadFullResolution(const Material& material);

        static Residency residency;

    private:
        class Entry;

    };

}
