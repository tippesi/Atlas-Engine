#pragma once

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture.h"
#include "texture/Texture2DArray.h"
#include "loader/ImageLoader.h"
#include "resource/Resource.h"
#include "common/Ref.h"
#include "materialgraph/MaterialGraph.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Atlas {

    typedef uint32_t MaterialUsage;

    typedef enum MaterialUsageBits {
        MeshBit = (1 << 0),
        TerrainBit = (1 << 1),
        ImpostorBit = (1 << 2)
    } MaterialUsageBits;

    class Material {

    public:
        Material();

        // Whether the material has the map, which might still be loading or have failed. Code binding a map checks IsLoaded
        bool HasBaseColorMap() const;
        bool HasOpacityMap() const;
        bool HasNormalMap() const;
        bool HasRoughnessMap() const;
        bool HasMetalnessMap() const;
        bool HasAoMap() const;
        bool HasDisplacementMap() const;
        bool HasEmissiveMap() const;

        std::array<ResourceHandle<Texture::Texture2D>, 8> GetMaps() const;

        bool HasGraph() const;

        // Materials with a graph have the blend mode and two sidedness of the graph
        MaterialBlendMode GetBlendMode() const;

        bool IsTwoSided() const;

        bool IsTranslucent() const { return GetBlendMode() != MaterialBlendMode::None; }

        // Alpha blended with a thickness and a refractive index other than 1, graphs decide it with their surface pins
        bool Refracts() const;

        // Graphs saved before they held the material type and the volume of the surface take them from the first material
        // which loads them
        void MigrateGraphSettings();

        // All textures the parameters of the graph use, with the ones of this material where it has its own
        std::vector<ResourceHandle<Texture::Texture2D>> GetGraphTextures() const;

        // The ones of this material, otherwise the defaults of the graph
        vec4 GetGraphValue(const MaterialGraph::Parameter& parameter) const;

        const ResourceHandle<Texture::Texture2D>& GetGraphTexture(const MaterialGraph::Parameter& parameter) const;

        // The value of a parameter of the graph in this material, the graph holds the default
        void SetGraphParameter(const std::string& parameter, vec4 value);

        void SetGraphTexture(const std::string& parameter, const ResourceHandle<Texture::Texture2D>& texture);

        // Back to the default of the graph
        void ResetGraphParameter(const std::string& parameter);

        std::string name;

        ResourceHandle<Texture::Texture2D> baseColorMap;
        ResourceHandle<Texture::Texture2D> opacityMap;
        ResourceHandle<Texture::Texture2D> normalMap;
        ResourceHandle<Texture::Texture2D> roughnessMap;
        ResourceHandle<Texture::Texture2D> metalnessMap;
        ResourceHandle<Texture::Texture2D> aoMap;
        ResourceHandle<Texture::Texture2D> displacementMap;
        ResourceHandle<Texture::Texture2D> emissiveMap;

        vec3 baseColor = vec3(1.0f);
        vec3 transmissiveColor = vec3(0.0f);

        vec3 emissiveColor = vec3(0.0f);
        float emissiveIntensity = 1.0f;

        float opacity = 1.0f;

        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;

        float reflectance = 0.5f;

        float normalScale = 0.5f;
        float displacementScale = 0.01f;

        float tiling = 1.0f;

        bool twoSided = false;
        bool vertexColors = false;

        // Translucent materials are blended in a forward pass instead of being cut out, the opacity is their coverage
        MaterialBlendMode blendMode = MaterialBlendMode::None;
        // Alpha blended surfaces with a thickness refract the background: closed one sided meshes are as thick as the
        // distance to their back faces, two sided ones are slabs of the thickness in meters, it also is the one of open
        // meshes like water planes. Zero doesn't refract
        float refractiveIndex = 1.5f;
        float thickness = 0.0f;
        // Refracting surfaces absorb light inside them like the volume of glTF materials: after the attenuation distance in
        // meters the light passing them has the attenuation color. Zero doesn't absorb
        vec3 attenuationColor = vec3(1.0f);
        float attenuationDistance = 0.0f;

        vec2 uvAnimation = vec2(0.0f);

        uint32_t uvChannel = 0;

        std::atomic_uint32_t usage = 0;

        // Generates the shaders of this material if set. With a surface in the graph the other properties
        // are only used by ray tracing, which doesn't run graphs. Behind the members every frame reads
        ResourceHandle<MaterialGraph::Graph> graph;
        std::unordered_map<std::string, vec4> graphValues;
        std::unordered_map<std::string, ResourceHandle<Texture::Texture2D>> graphTextures;
        // Changes with the values and textures above, renderers only read them again after a change
        uint32_t graphParameterVersion = 0;
        MaterialGraph::BakeSettings graphBake;

    };


}
