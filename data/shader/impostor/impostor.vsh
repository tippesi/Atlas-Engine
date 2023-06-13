#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/octahedron.hsh>
#include <../globals.hsh>

layout(location=0) in vec2 vPosition;

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout(std430, set = 1, binding = 2) buffer Matrices {
	mat4 matrices[];
};

layout (std430, set = 3, binding = 3) buffer ViewPlanes {
	ViewPlane viewPlanes[];
};

layout(location=0) out vec3 positionVS;
layout(location=1) out vec2 texCoordVS;
layout(location=2) out vec3 ndcCurrentVS;
layout(location=3) out vec3 ndcLastVS;

#ifdef INTERPOLATION
layout(location=4) flat out int index0VS;
layout(location=5) flat out int index1VS;
layout(location=6) flat out int index2VS;

layout(location=7) flat out float weight0VS;
layout(location=8) flat out float weight1VS;
layout(location=9) flat out float weight2VS;
#else
layout(location=4) flat out int indexVS;
#endif

layout(push_constant) uniform constants {
	vec4 center;

	float radius;
	int views;
	float cutoff;
	uint materialIdx;
} PushConstants;

const bool hemiOctahedron = true;

vec4 InterpolateTriangle(vec2 coord) {

	vec4 weights;

	coord = fract(coord);

	weights.x = min(1.0 - coord.x, 1.0 - coord.y);
	weights.y = abs(dot(vec2(1.0, -1.0), coord));
	weights.z = min(coord.x, coord.y);

	weights.w = saturate(ceil(coord.x - coord.y));

	return weights;

}

void main() {
	
	mat4 mMatrix = matrices[gl_InstanceIndex];
    texCoordVS = 0.5 * vPosition + 0.5;
	
	vec3 pos = vec3(mMatrix * vec4(0.0, 0.0, 0.0, 1.0));
	vec3 dir = normalize(globalData.cameraLocation.xyz - pos);

    float frames = float(PushConstants.views);

	vec2 octahedron;
	if (hemiOctahedron) {
		octahedron = UnitVectorToHemiOctahedron(dir);
	}
	else {
		octahedron = UnitVectorToOctahedron(dir);
	}

#ifdef INTERPOLATION
	vec2 coord = octahedron * (frames - 1.0);

	vec4 weights = InterpolateTriangle(coord);

	if (weights.w < 1.0) {
		index0VS = Flatten2D(ivec2(coord), ivec2(frames));
		index1VS = Flatten2D(ivec2(coord) + ivec2(0, 1), ivec2(frames));
		index2VS = Flatten2D(ivec2(coord) + ivec2(1, 1), ivec2(frames));
		weight0VS = weights.x;
		weight1VS = weights.y;
		weight2VS = weights.z;
	}
	else {
		index0VS = Flatten2D(ivec2(coord), ivec2(frames));
		index1VS = Flatten2D(ivec2(coord) + ivec2(1, 0), ivec2(frames));
		index2VS = Flatten2D(ivec2(coord) + ivec2(1, 1), ivec2(frames));
		weight0VS = weights.x;
		weight1VS = weights.y;
		weight2VS = weights.z;
	}
#else
	vec2 coord = floor(octahedron * (frames - 1.0));
	indexVS = Flatten2D(ivec2(coord), ivec2(frames));
#endif
	
    vec2 position = vPosition.xy * PushConstants.radius;

	vec4 up, right;

#ifdef INTERPOLATION
	// Maybe use the camera view plane here?
    ViewPlane viewPlane0 = viewPlanes[index0VS];
	ViewPlane viewPlane1 = viewPlanes[index1VS];
	ViewPlane viewPlane2 = viewPlanes[index2VS];

	up = weight0VS * viewPlane0.up + 
		weight1VS * viewPlane1.up + 
		weight2VS * viewPlane2.up;

	right = weight0VS * viewPlane0.right + 
		weight1VS * viewPlane1.right + 
		weight2VS * viewPlane2.right;
#else
	ViewPlane viewPlane = viewPlanes[indexVS];

	up = viewPlane.up;
	right = viewPlane.right;
#endif

    vec4 modelPosition = vec4((normalize(up.xyz) * position.y
        + normalize(right.xyz) * position.x) + PushConstants.center.xyz, 1.0);
	positionVS = vec3(globalData.vMatrix * mMatrix * modelPosition);

    gl_Position =  globalData.pMatrix * vec4(positionVS, 1.0);

    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
    vec4 last = globalData.pvMatrixLast * mMatrix * modelPosition;
	ndcLastVS = vec3(last.xy, last.w);

}
