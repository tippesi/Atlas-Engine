#include <ddgi.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec3 normalFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

in vec3 positionVS;
in vec3 normalVS;

in vec3 ndcCurrentVS;
in vec3 ndcLastVS;

in flat uint instanceID;
in vec3 worldSpaceNormal;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

uniform uint probeMaterialIdx;
uniform uint probeActiveMaterialIdx;
uniform uint probeInactiveMaterialIdx;
uniform uint probeOffsetMaterialIdx;

void main() {

	vec2 momRes = vec2(16);
	vec2 totalResolution = vec2(volumeProbeCount.xz) * momRes;
	vec2 momTexelSize = 1.0 / totalResolution;

	ivec3 probeCoord = GetProbeGridCoord(instanceID);
	vec3 probeOffset = GetProbeOffset(instanceID);
	vec3 probePosition = GetProbePosition(probeCoord);

	vec2 momOctCoord = UnitVectorToOctahedron(worldSpaceNormal);
	vec3 momCoord = GetProbeCoord(probeCoord, momOctCoord, momRes, momTexelSize, 14);
	vec2 moments = textureLod(momentsVolume, momCoord, 0).rg;

	baseColorFS = vec3(moments.x) / length(cellSize);
	
	geometryNormalFS = normalize(normalVS);
	geometryNormalFS = 0.5 * geometryNormalFS + 0.5;

	float roughnessFactor = 1.0;
	float metalnessFactor = 1.0;
	float aoFactor = 1.0;

	// Calculate velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;

	materialIdxFS = GetProbeState(instanceID) == PROBE_STATE_ACTIVE ? probeActiveMaterialIdx : probeInactiveMaterialIdx;
	materialIdxFS = moments.x < 0.6 ? probeOffsetMaterialIdx : materialIdxFS;
	
}