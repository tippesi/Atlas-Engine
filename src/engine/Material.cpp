#include "Material.h"

namespace Atlas {

    Material::Material() {



    }

    bool Material::HasBaseColorMap() const {

        return baseColorMap.IsValid();

    }

    bool Material::HasOpacityMap() const {

        return opacityMap.IsValid();

    }

    bool Material::HasNormalMap() const {

        return normalMap.IsValid();

    }

    bool Material::HasRoughnessMap() const {

        return roughnessMap.IsValid();

    }

    bool Material::HasMetalnessMap() const {

        return metalnessMap.IsValid();

    }

    bool Material::HasAoMap() const {

        return aoMap.IsValid();

    }

    bool Material::HasDisplacementMap() const {

        return displacementMap.IsValid();

    }

    bool Material::HasEmissiveMap() const {

        return emissiveMap.IsValid();

    }

    std::array<ResourceHandle<Texture::Texture2D>, 8> Material::GetMaps() const {

        return { baseColorMap, opacityMap, normalMap, roughnessMap, metalnessMap, aoMap, displacementMap, emissiveMap };

    }

    bool Material::HasGraph() const {

        return graph.IsLoaded();

    }

    std::vector<ResourceHandle<Texture::Texture2D>> Material::GetGraphTextures() const {

        std::vector<ResourceHandle<Texture::Texture2D>> textures;
        if (!HasGraph())
            return textures;

        auto compiled = graph->GetCompiled();
        if (!compiled)
            return textures;

        for (const auto& parameter : compiled->parameters) {
            if (parameter.type != MaterialGraph::ValueType::Texture)
                continue;

            const auto& texture = GetGraphTexture(parameter);
            if (texture.IsValid())
                textures.push_back(texture);
        }

        return textures;

    }

    vec4 Material::GetGraphValue(const MaterialGraph::Parameter& parameter) const {

        auto iter = graphValues.find(parameter.name);
        return iter != graphValues.end() ? iter->second : parameter.defaultValue;

    }

    const ResourceHandle<Texture::Texture2D>& Material::GetGraphTexture(const MaterialGraph::Parameter& parameter) const {

        auto iter = graphTextures.find(parameter.name);
        return iter != graphTextures.end() ? iter->second : parameter.defaultTexture;

    }

    void Material::SetGraphParameter(const std::string& parameter, vec4 value) {

        // Scripts often set the same value every frame
        auto iter = graphValues.find(parameter);
        if (iter != graphValues.end() && iter->second == value)
            return;

        graphValues[parameter] = value;
        graphParameterVersion++;

    }

    void Material::SetGraphTexture(const std::string& parameter, const ResourceHandle<Texture::Texture2D>& texture) {

        graphTextures[parameter] = texture;
        graphParameterVersion++;

    }

    void Material::ResetGraphParameter(const std::string& parameter) {

        graphTextures.erase(parameter);
        graphValues.erase(parameter);
        graphParameterVersion++;

    }

}
