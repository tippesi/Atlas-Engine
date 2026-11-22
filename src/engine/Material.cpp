#include "Material.h"
#include "common/ColorConverter.h"
#include "common/MathHelper.h"
#include "materialgraph/NodeLibrary.h"

#include <algorithm>

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

    MaterialBlendMode Material::GetBlendMode() const {

        return HasGraph() ? graph->blendMode : blendMode;

    }

    bool Material::IsTwoSided() const {

        return HasGraph() ? graph->twoSided : twoSided;

    }

    bool Material::Refracts() const {

        if (GetBlendMode() != MaterialBlendMode::Alpha)
            return false;
        if (!HasGraph())
            return thickness > 0.0f && refractiveIndex != 1.0f;

        auto compiled = graph->GetCompiled();
        return compiled && compiled->valid && compiled->refracts;

    }

    void Material::MigrateGraphSettings() {

        if (!HasGraph() || !graph->legacyMaterialSettings.exchange(false))
            return;

        graph->blendMode = blendMode;
        graph->twoSided = twoSided;

        // The pins of a surface replace these values, unlinked they get the ones the material had
        auto surface = std::find_if(graph->nodes.begin(), graph->nodes.end(),
            [](const MaterialGraph::Node& node) { return node.type == MaterialGraph::NodeLibrary::surfaceOutputType; });
        if (surface != graph->nodes.end()) {
            using MaterialGraph::NodeLibrary;
            auto& values = surface->inputValues;
            values[NodeLibrary::reflectancePin] = vec4(reflectance);
            // The colors of materials are sRGB, the values of graphs linear. The transmission of graphs has the base color
            auto linearTransmission = Common::ColorConverter::ConvertSRGBToLinear(transmissiveColor);
            auto linearBaseColor = Common::ColorConverter::ConvertSRGBToLinear(baseColor);
            auto maxComponent = [](vec3 color) { return glm::max(color.r, glm::max(color.g, color.b)); };
            auto transmission = maxComponent(linearBaseColor) > 0.0f ? maxComponent(linearTransmission) / maxComponent(linearBaseColor) : 0.0f;
            values[NodeLibrary::transmissionPin] = vec4(Helper::Math::Saturate(transmission));
            values[NodeLibrary::refractiveIndexPin] = vec4(thickness > 0.0f ? refractiveIndex : 1.0f);
            values[NodeLibrary::thicknessPin] = vec4(thickness);
            values[NodeLibrary::attenuationColorPin] = vec4(Common::ColorConverter::ConvertSRGBToLinear(attenuationColor), 0.0f);
            values[NodeLibrary::attenuationDistancePin] = vec4(attenuationDistance);
        }
        graph->Compile();

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
