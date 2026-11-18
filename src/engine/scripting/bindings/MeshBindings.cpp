#include "MeshBindings.h"

#include "mesh/Mesh.h"

namespace Atlas::Scripting::Bindings {

    void GenerateMeshBindings(sol::table* ns) {

        ns->new_enum<Mesh::MeshMobility>("MeshMobility", {
            { "Stationary", Mesh::MeshMobility::Stationary },
            { "Movable", Mesh::MeshMobility::Movable },
        });

        ns->new_enum<Mesh::MeshUsageBits>("MeshUsageBits", {
            { "MultiBufferedBit", Mesh::MeshUsageBits::MultiBufferedBit },
            { "HostAccessBit", Mesh::MeshUsageBits::HostAccessBit },
        });

        ns->new_usertype<Mesh::Joint>("Joint",
            "name", &Mesh::Joint::name,
            "parentIdx", &Mesh::Joint::parentIdx,
            "inverseBindMatrix", &Mesh::Joint::inverseBindMatrix,
            "position", &Mesh::Joint::position,
            "rotation", &Mesh::Joint::rotation,
            "scale", &Mesh::Joint::scale,
            "aabb", &Mesh::Joint::aabb,
            "hasVertices", &Mesh::Joint::hasVertices
            );

        ns->new_usertype<Mesh::Skeleton>("Skeleton",
            "GetJointIndex", &Mesh::Skeleton::GetJointIndex,
            "GetJointCount", &Mesh::Skeleton::GetJointCount,
            "UpdateRestPose", &Mesh::Skeleton::UpdateRestPose,
            "joints", &Mesh::Skeleton::joints,
            "restJointMatrices", &Mesh::Skeleton::restJointMatrices,
            "restAABB", &Mesh::Skeleton::restAABB
            );

        ns->new_usertype<Mesh::MeshLodSettings>("MeshLodSettings",
            sol::constructors<Mesh::MeshLodSettings()>(),
            "maxLodCount", &Mesh::MeshLodSettings::maxLodCount,
            "reduction", &Mesh::MeshLodSettings::reduction,
            "minReduction", &Mesh::MeshLodSettings::minReduction
            );

        ns->new_usertype<Mesh::SoftBodyMeshSettings>("SoftBodyMeshSettings",
            sol::constructors<Mesh::SoftBodyMeshSettings()>(),
            "maxSimulatedVertices", &Mesh::SoftBodyMeshSettings::maxSimulatedVertices,
            "keepBorders", &Mesh::SoftBodyMeshSettings::keepBorders
            );

        ns->new_usertype<Mesh::MeshData>("MeshData",
            "IsSkinned", &Mesh::MeshData::IsSkinned,
            "lodErrors", sol::readonly(&Mesh::MeshData::lodErrors),
            "lodSettings", sol::readonly(&Mesh::MeshData::lodSettings),
            "filename", &Mesh::MeshData::name,
            "materials", &Mesh::MeshData::materials,
            "animations", &Mesh::MeshData::animations,
            "skeleton", &Mesh::MeshData::skeleton,
            "primitiveType", &Mesh::MeshData::primitiveType,
            "aabb", &Mesh::MeshData::aabb,
            "radius", &Mesh::MeshData::radius
            );
        
        ns->new_usertype<Mesh::Mesh>("Mesh",
            "UpdateData", &Mesh::Mesh::UpdateData,
            "UpdatePipelines", &Mesh::Mesh::UpdatePipelines,
            "UpdateVertexArray", &Mesh::Mesh::UpdateVertexArray,
            "GenerateLods", [](Mesh::Mesh& mesh, sol::optional<Mesh::MeshLodSettings> settings) {
                mesh.GenerateLods(settings.value_or(Mesh::MeshLodSettings()));
            },
            "ClearLods", &Mesh::Mesh::ClearLods,
            "ResetSoftBodySimulationMesh", &Mesh::Mesh::ResetSoftBodySimulationMesh,
            "IsGeneratingLods", &Mesh::Mesh::IsGeneratingLods,
            "name", &Mesh::Mesh::name,
            "data", &Mesh::Mesh::data,
            "mobility", &Mesh::Mesh::mobility,
            "usage", &Mesh::Mesh::usage,
            "cullBackFaces", &Mesh::Mesh::cullBackFaces,
            "depthTest", &Mesh::Mesh::depthTest,
            "castShadow", &Mesh::Mesh::castShadow,
            "rayTrace", &Mesh::Mesh::rayTrace,
            "vegetation", &Mesh::Mesh::vegetation,
            "windNoiseTextureLod", &Mesh::Mesh::windNoiseTextureLod,
            "windBendScale", &Mesh::Mesh::windBendScale,
            "windWiggleScale", &Mesh::Mesh::windWiggleScale,
            "allowedShadowCascades", &Mesh::Mesh::allowedShadowCascades,
            "distanceCulling", &Mesh::Mesh::distanceCulling,
            "shadowDistanceCulling", &Mesh::Mesh::shadowDistanceCulling,
            "rayTraceDistanceCulling", &Mesh::Mesh::rayTraceDistanceCulling,
            "impostorDistance", &Mesh::Mesh::impostorDistance,
            "impostorShadowDistance", &Mesh::Mesh::impostorShadowDistance,
            "minScreenSize", &Mesh::Mesh::minScreenSize,
            "lodPixelError", &Mesh::Mesh::lodPixelError,
            "animationLodDistance", &Mesh::Mesh::animationLodDistance,
            "animationMaxInterval", &Mesh::Mesh::animationMaxInterval,
            "invertUVs", &Mesh::Mesh::invertUVs,
            "softBody", &Mesh::Mesh::softBody
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
            "vertexColors", &Material::vertexColors,
            "baseColorMap", &Material::baseColorMap,
            "opacityMap", &Material::opacityMap,
            "normalMap", &Material::normalMap,
            "roughnessMap", &Material::roughnessMap,
            "metalnessMap", &Material::metalnessMap,
            "aoMap", &Material::aoMap,
            "displacementMap", &Material::displacementMap,
            "SetGraphParameter", sol::overload(
                &Material::SetGraphParameter,
                [](Material& material, const std::string& parameter, float value) { material.SetGraphParameter(parameter, vec4(value)); },
                [](Material& material, const std::string& parameter, const vec2& value) { material.SetGraphParameter(parameter, vec4(value, 0.0f, 0.0f)); },
                [](Material& material, const std::string& parameter, const vec3& value) { material.SetGraphParameter(parameter, vec4(value, 0.0f)); }
            )
            );

    }

}