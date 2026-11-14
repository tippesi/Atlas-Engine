#pragma once

#include "MaterialGraph.h"

#include "../Material.h"
#include "../mesh/Mesh.h"

#include <array>
#include <string>

namespace Atlas::MaterialGraph {

    /**
     * Writes the surface of a graph into the properties of its material, which ray tracing uses since it doesn't run graphs.
     * The result is frozen: maps show one point in time for the positions and texture coordinates of one mesh, averages
     * lose all detail but fit every mesh. Maps go next to the material, into baked/<material>_<property>.png. A property
     * either gets a map or a value afterwards, whatever it had before comes back when the bake gets removed.
     */
    class Baker {

    public:
        struct Result {
            bool success = false;
            std::string error;
            // Properties which got a map, others got a single value since they don't vary
            int32_t maps = 0;
        };

        // Uses material->graphBake. Blocks until the GPU is done, the material still has to be saved afterwards
        static Result Bake(const ResourceHandle<Material>& material, const ResourceHandle<Mesh::Mesh>& mesh);

        // Brings back the properties from before the bake, the files of its maps stay
        static void Clear(Material& material);

        // Changes with everything a bake depends on, to find out whether it is outdated
        static Hash GetState(const Material& material);

    private:
        struct BakedMap {
            std::string name;
            ResourceHandle<Texture::Texture2D>* handle;
        };

        static std::array<BakedMap, 7> GetBakedMaps(Material& material);

        // Keeps what the first bake replaces and removes all maps a bake writes
        static void PrepareProperties(Material& material);

        // Without the maps
        static UnbakedProperties GetProperties(const Material& material);

        static void SetProperties(Material& material, const UnbakedProperties& properties);

    };

}
