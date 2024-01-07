#include <../globals.hsh>
#include <ddgi.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec3 normalFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

layout(location=0) in vec3 normalVS;

layout(location=1) in vec3 ndcCurrentVS;
layout(location=2) in vec3 ndcLastVS;

layout(location=3) in flat uint instanceID;
layout(location=4) in vec3 worldSpaceNormal;

layout(push_constant) uniform constants {
    uint probeMaterialIdx;
    uint probeActiveMaterialIdx;
    uint probeInactiveMaterialIdx;
    uint probeOffsetMaterialIdx;
} pushConstants;

void main() {

    vec2 momRes = vec2(ddgiData.volumeMomentsRes) + 2.0;
    vec2 totalResolution = vec2(ddgiData.volumeProbeCount.xz) * momRes;
    vec2 momTexelSize = 1.0 / totalResolution;

    ivec3 probeCoord = GetProbeGridCoord(instanceID);
    vec3 probeOffset = GetProbeOffset(instanceID);
    vec3 probePosition = GetProbePosition(probeCoord);

    vec2 momOctCoord = UnitVectorToOctahedron(worldSpaceNormal);
    vec3 momCoord = GetProbeCoord(probeCoord, momOctCoord, momRes, momTexelSize, 14);
    vec2 moments = textureLod(momentsVolume, momCoord, 0).rg;

    baseColorFS = vec3(moments.x) / length(ddgiData.cellSize.xyz);
    
    geometryNormalFS = normalize(normalVS);
    geometryNormalFS = 0.5 * geometryNormalFS + 0.5;

    float roughnessFactor = 1.0;
    float metalnessFactor = 1.0;
    float aoFactor = 1.0;

    // Calculate velocity
    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData[0].jitterLast;
    ndcC -= globalData[0].jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = GetProbeState(instanceID) == PROBE_STATE_ACTIVE ?
        pushConstants.probeActiveMaterialIdx : pushConstants.probeInactiveMaterialIdx;
    materialIdxFS = moments.x < 0.6 ? pushConstants.probeOffsetMaterialIdx : materialIdxFS;
    
}