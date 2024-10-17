#include "MeshSerializer.h"
#include "resource/ResourceManager.h"
#include "loader/MaterialLoader.h"

namespace Atlas::Mesh {

    void to_json(json& j, const Mesh& p) {

        int mobility = static_cast<int>(p.mobility);
        int usage = static_cast<int>(p.usage);

        j = json {
            {"name", p.name},
            {"mobility", mobility},
            {"usage", usage},
            {"cullBackFaces", p.cullBackFaces},
            {"rayTrace", p.rayTrace},
            {"castShadow", p.castShadow},
            {"vegetation", p.vegetation},
            {"windNoiseTextureLod", p.windNoiseTextureLod},
            {"windBendScale", p.windBendScale},
            {"windWiggleScale", p.windWiggleScale},
            {"allowedShadowCascades", p.allowedShadowCascades},
            {"distanceCulling", p.distanceCulling},
            {"shadowDistanceCulling", p.shadowDistanceCulling},
            {"rayTraceDistanceCulling", p.rayTraceDistanceCulling},
            {"impostorDistance", p.impostorDistance},
            {"impostorShadowDistance", p.impostorShadowDistance},
            {"invertUVs", p.invertUVs},
            {"data", p.data},
        };
    }

    void from_json(const json& j, Mesh& p) {

        int mobility, usage;
        
        j.at("name").get_to(p.name);
        
        j.at("mobility").get_to(mobility);
        j.at("usage").get_to(usage);
        j.at("cullBackFaces").get_to(p.cullBackFaces);
        j.at("castShadow").get_to(p.castShadow);
        j.at("vegetation").get_to(p.vegetation);
        j.at("windNoiseTextureLod").get_to(p.windNoiseTextureLod);
        j.at("windBendScale").get_to(p.windBendScale);
        j.at("windWiggleScale").get_to(p.windWiggleScale);
        j.at("allowedShadowCascades").get_to(p.allowedShadowCascades);
        j.at("distanceCulling").get_to(p.distanceCulling);
        j.at("shadowDistanceCulling").get_to(p.shadowDistanceCulling);
        j.at("impostorDistance").get_to(p.impostorDistance);
        j.at("impostorShadowDistance").get_to(p.impostorShadowDistance);
        j.at("invertUVs").get_to(p.invertUVs);
        j.at("data").get_to(p.data);

        try_get_json(j, "rayTrace", p.rayTrace);
        try_get_json(j, "rayTraceDistanceCulling", p.rayTraceDistanceCulling);

        p.mobility = static_cast<MeshMobility>(mobility);
        p.usage = static_cast<MeshUsage>(usage);

        p.UpdateData();

    }

    void to_json(json& j, const MeshData& p) {

        bool binary = true;

        j = json{
           {"name", p.name},
           {"indexCount", p.GetIndexCount()},
           {"vertexCount", p.GetVertexCount()},
           {"indices", p.indices},
           {"vertices", p.vertices},
           {"texCoords", p.texCoords},
           {"normals", p.normals},
           {"tangents", p.tangents},
           {"colors", p.colors},
           {"subData", p.subData},
           {"primitiveType", p.primitiveType},
           {"radius", p.radius},
           {"aabb", p.aabb},
           {"binary", binary},
        };

        to_json(j["indices"], p.indices, binary);
        to_json(j["vertices"], p.vertices, binary);
        to_json(j["texCoords"], p.texCoords, binary);
        to_json(j["normals"], p.normals, binary);
        to_json(j["tangents"], p.tangents, binary);
        to_json(j["colors"], p.colors, binary);

        for (size_t i = 0; i < p.materials.size(); i++) {
            auto& material = p.materials[i];
            auto path = material.GetResource()->path;
            j["materials"][i] = path;
        }

    }

    void from_json(const json& j, MeshData& p) {

        bool binary;
        int32_t indexCount, vertexCount;

        j.at("name").get_to(p.name);
        j.at("indexCount").get_to(indexCount);
        j.at("vertexCount").get_to(vertexCount);

        p.SetIndexCount(indexCount);
        p.SetVertexCount(vertexCount);

        j.at("subData").get_to(p.subData);
        j.at("primitiveType").get_to(p.primitiveType);
        j.at("radius").get_to(p.radius);
        j.at("aabb").get_to(p.aabb);
        j.at("binary").get_to(binary);

        from_json(j["indices"], p.indices, binary);
        from_json(j["vertices"], p.vertices, binary);
        from_json(j["texCoords"], p.texCoords, binary);
        from_json(j["normals"], p.normals, binary);
        from_json(j["tangents"], p.tangents, binary);
        from_json(j["colors"], p.colors, binary);

        if (j.contains("materials")) {
            for (auto& path : j["materials"]) {
                auto material = ResourceManager<Material>::GetOrLoadResourceWithLoader(path,
                    ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
                p.materials.push_back(material);
            }

            for (auto& subData : p.subData) {
                subData.material = p.materials[subData.materialIdx];
            }
        }

    }

    void to_json(json& j, const MeshSubData& p) {

        j = json{
           {"name", p.name},
           {"indicesOffset", p.indicesOffset},
           {"indicesCount", p.indicesCount},
           {"materialIdx", p.materialIdx},
           {"aabb", p.aabb},
        };

    }

    void from_json(const json& j, MeshSubData& p) {

        j.at("name").get_to(p.name);
        j.at("indicesOffset").get_to(p.indicesOffset);
        j.at("indicesCount").get_to(p.indicesCount);
        j.at("materialIdx").get_to(p.materialIdx);
        j.at("aabb").get_to(p.aabb);

    }

    void to_json(json& j, const Impostor& p) {



    }

    void from_json(const json& j, Impostor& p) {



    }

}

namespace Atlas {

    void to_json(json& j, const Material& p) {

        j = json{
            {"name", p.name},
            {"baseColor", p.baseColor},
            {"transmissiveColor", p.transmissiveColor},
            {"emissiveColor", p.emissiveColor},
            {"emissiveIntensity", p.emissiveIntensity},
            {"opacity", p.opacity},
            {"roughness", p.roughness},
            {"metalness", p.metalness},
            {"ao", p.ao},
            {"reflectance", p.reflectance},
            {"normalScale", p.normalScale},
            {"displacementScale", p.displacementScale},
            {"tiling", p.tiling},
            {"twoSided", p.twoSided},
            {"vertexColors", p.vertexColors},
            {"uvChannel", p.uvChannel},
            {"uvAnimation", p.uvAnimation},
        };

        if (p.baseColorMap.IsValid())
            j["baseColorMapPath"] = p.baseColorMap.GetResource()->path;
        if (p.opacityMap.IsValid())
            j["opacityMapPath"] = p.opacityMap.GetResource()->path;
        if (p.normalMap.IsValid())
            j["normalMapPath"] = p.normalMap.GetResource()->path;
        if (p.roughnessMap.IsValid())
            j["roughnessMapPath"] = p.roughnessMap.GetResource()->path;
        if (p.metalnessMap.IsValid())
            j["metalnessMapPath"] = p.metalnessMap.GetResource()->path;
        if (p.aoMap.IsValid())
            j["aoMapPath"] = p.aoMap.GetResource()->path;
        if (p.displacementMap.IsValid())
            j["displacementMapPath"] = p.displacementMap.GetResource()->path;
        if (p.emissiveMap.IsValid())
            j["emissiveMapPath"] = p.emissiveMap.GetResource()->path;

    }

    void from_json(const json& j, Material& p) {

        j.at("name").get_to(p.name);
        j.at("baseColor").get_to(p.baseColor);
        j.at("transmissiveColor").get_to(p.transmissiveColor);
        j.at("emissiveColor").get_to(p.emissiveColor);
        j.at("emissiveIntensity").get_to(p.emissiveIntensity);
        j.at("opacity").get_to(p.opacity);
        j.at("roughness").get_to(p.roughness);
        j.at("metalness").get_to(p.metalness);
        j.at("ao").get_to(p.ao);
        j.at("reflectance").get_to(p.reflectance);
        j.at("normalScale").get_to(p.normalScale);
        j.at("displacementScale").get_to(p.displacementScale);
        j.at("tiling").get_to(p.tiling);
        j.at("tiling").get_to(p.tiling);
        j.at("twoSided").get_to(p.twoSided);
        j.at("vertexColors").get_to(p.vertexColors);
        j.at("uvChannel").get_to(p.uvChannel);

        try_get_json(j, "uvAnimation", p.uvAnimation);

        auto getTextureHandle = [](const std::string& path, bool colorSpaceConversion, bool priority = false) -> auto {
            return ResourceManager<Texture::Texture2D>::GetOrLoadResource(path, colorSpaceConversion,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic, 0, false, priority);
            };

        if (j.contains("baseColorMapPath"))
            p.baseColorMap = getTextureHandle(j["baseColorMapPath"], false);
        if (j.contains("opacityMapPath"))
            p.opacityMap = getTextureHandle(j["opacityMapPath"], false, true);
        if (j.contains("normalMapPath"))
            p.normalMap = getTextureHandle(j["normalMapPath"], false);
        if (j.contains("roughnessMapPath"))
            p.roughnessMap = getTextureHandle(j["roughnessMapPath"], false);
        if (j.contains("metalnessMapPath"))
            p.metalnessMap = getTextureHandle(j["metalnessMapPath"], false);
        if (j.contains("aoMapPath"))
            p.aoMap = getTextureHandle(j["aoMapPath"], false);
        if (j.contains("displacementMapPath"))
            p.displacementMap = getTextureHandle(j["displacementMapPath"], false);
        if (j.contains("emissiveMapPath"))
            p.emissiveMap = getTextureHandle(j["emissiveMapPath"], false);

    }

}