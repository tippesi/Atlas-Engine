#include "MeshSerializer.h"
#include "loader/MeshDataLoader.h"


namespace Atlas::Mesh {

    void to_json(json& j, const Impostor& p) {



    }

    void from_json(const json& j, Impostor& p) {



    }

    void to_json(json& j, const Mesh& p) {

        int mobility = static_cast<int>(p.mobility);

        j = json {
            {"name", p.name},
            {"mobility", p.mobility},
            {"subDataIndices", p.subDataIndices},
            {"radius", p.radius},
            {"aabb", p.aabb},
            {"cullBackFaces", p.cullBackFaces},
            {"castShadow", p.castShadow},
            {"vegetation", p.vegetation},
            {"windNoiseTextureLod", p.windNoiseTextureLod},
            {"windBendScale", p.windBendScale},
            {"windWiggleScale", p.windWiggleScale},
            {"allowedShadowCascades", p.allowedShadowCascades},
            {"distanceCulling", p.distanceCulling},
            {"shadowDistanceCulling", p.shadowDistanceCulling},
            {"impostorDistance", p.impostorDistance},
            {"impostorShadowDistance", p.impostorShadowDistance},
            {"invertUVs", p.invertUVs},
            {"compactedData", p.compactedData},
        };

        if (p.data.IsValid()) {
            j["resourcePath"] = p.data.GetResource()->path;
        }

    }

    void from_json(const json& j, Mesh& p) {

        int mobility;
        
        j.at("name").get_to(p.name);
        j.at("mobility").get_to(p.mobility);
        j.at("subDataIndices").get_to(p.subDataIndices);
        j.at("radius").get_to(p.radius);
        j.at("aabb").get_to(p.aabb);
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
        j.at("compactedData").get_to(p.compactedData);

        p.mobility = static_cast<MeshMobility>(mobility);

        if (j.contains("resourcePath")) {
            std::string resourcePath;
            j.at("resourcePath").get_to(resourcePath);

            p.data = ResourceManager<MeshData>::GetOrLoadResourceWithLoader(resourcePath,
                ResourceOrigin::User, Loader::MeshDataLoader::LoadMeshData, false, 8192);
        }

    }

}