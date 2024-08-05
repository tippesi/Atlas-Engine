#include "MeshBindings.h"

#include "mesh/Mesh.h"

namespace Atlas::Scripting::Bindings {

    void GenerateMeshBindings(sol::table* ns) {

        ns->new_enum<Mesh::MeshMobility>("MeshMobility", {
            { "Stationary", Mesh::MeshMobility::Stationary },
            { "Movable", Mesh::MeshMobility::Movable },
        });

        ns->new_enum<Mesh::MeshDataUsageBits>("MeshUsageBits", {
            { "MultiBufferedBit", Mesh::MeshDataUsageBits::MultiBufferedBit },
            { "HostAccessBit", Mesh::MeshDataUsageBits::HostAccessBit },
        });

        ns->new_usertype<Mesh::MeshData>("MeshData",
            "filename", &Mesh::MeshData::filename,
            "usage", &Mesh::MeshData::usage,
            "materials", &Mesh::MeshData::materials,
            "primitiveType", &Mesh::MeshData::primitiveType,
            "aabb", &Mesh::MeshData::aabb,
            "transform", &Mesh::MeshData::transform,
            "radius", &Mesh::MeshData::radius
            );
        
        ns->new_usertype<Mesh::Mesh>("Mesh",
            "name", &Mesh::Mesh::name,
            "data", &Mesh::Mesh::data,
            "mobility", &Mesh::Mesh::mobility,
            "radius", &Mesh::Mesh::radius,
            "aabb", &Mesh::Mesh::aabb,
            "cullBackFaces", &Mesh::Mesh::cullBackFaces,
            "castShadow", &Mesh::Mesh::castShadow,
            "vegetation", &Mesh::Mesh::vegetation,
            "windNoiseTextureLod", &Mesh::Mesh::windNoiseTextureLod,
            "windBendScale", &Mesh::Mesh::windBendScale,
            "windWiggleScale", &Mesh::Mesh::windWiggleScale,
            "allowedShadowCascades", &Mesh::Mesh::allowedShadowCascades,
            "distanceCulling", &Mesh::Mesh::distanceCulling,
            "shadowDistanceCulling", &Mesh::Mesh::shadowDistanceCulling,
            "impostorDistance", &Mesh::Mesh::impostorDistance,
            "impostorShadowDistance", &Mesh::Mesh::impostorShadowDistance,
            "invertUVs", &Mesh::Mesh::invertUVs
            );

    }

    void GenerateMaterialBindings(sol::table* ns) {

         ns->new_usertype<Material>("Material",
            "HasBaseColorMap", &Material::HasBaseColorMap,
            "HasOpacityMap", &Material::HasOpacityMap,
            "HasNormalMap", &Material::HasNormalMap,
            "HasRoughnessMap", &Material::HasRoughnessMap,
            "HasMetalnessMap", &Material::HasMetalnessMap,
            "HasAoMap", &Material::HasAoMap,
            "HasDisplacementMap", &Material::HasDisplacementMap,
            "name", &Material::name,
            "baseColor", &Material::baseColor,
            "transmissiveColor", &Material::transmissiveColor,
            "emissiveColor", &Material::emissiveColor,
            "emissiveIntensity", &Material::emissiveIntensity,
            "opacity", &Material::opacity,
            "roughness", &Material::roughness,
            "metalness", &Material::metalness,
            "ao", &Material::ao,
            "reflectance", &Material::reflectance,
            "normalScale", &Material::normalScale,
            "displacementScale", &Material::displacementScale,
            "tiling", &Material::tiling,
            "twoSided", &Material::twoSided,
            "vertexColors", &Material::vertexColors
            );

    }

}