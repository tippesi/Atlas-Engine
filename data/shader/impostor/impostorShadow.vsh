#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/octahedron.hsh>

layout(location=0) in vec2 vPosition;

layout(location=0) out vec2 texCoordVS;
#ifdef INTERPOLATION
layout(location=1) flat out int index0VS;
layout(location=2) flat out int index1VS;
layout(location=3) flat out int index2VS;

layout(location=4) flat out float weight0VS;
layout(location=5) flat out float weight1VS;
layout(location=6) flat out float weight2VS;
#else
layout(location=1) flat out int indexVS;
#endif
#ifdef PIXEL_DEPTH_OFFSET
layout(location=7) out vec3 modelPositionVS;
layout(location=8) flat out vec3 instanceScale;
#endif

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout(std430, set = 1, binding = 3) readonly buffer Matrices {
    mat3x4 matrices[];
};

layout (std430, set = 3, binding = 2) readonly buffer ViewPlanes {
	ViewPlane viewPlanes[];
};

layout(set = 3, binding = 3, std140) uniform UniformBuffer{
	vec4 center;

	float radius;
	int views;

	float cutoff;
	float mipBias;
} uniforms;

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;

    vec4 lightLocation;
	vec4 lightUp;
	vec4 lightRight;
} pushConstants;

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

    mat4 mMatrix = mat4(transpose(matrices[gl_InstanceIndex]));

    texCoordVS = 0.5 * vPosition + 0.5;
	
	vec3 translation = vec3(mMatrix[3]);

	vec3 pos = translation + uniforms.center.xyz;
	vec3 dir = normalize(vec3(inverse(mMatrix) * vec4(pushConstants.lightLocation.xyz - pos, 0.0)));
    float frames = float(uniforms.views);

	vec2 octahedron = UnitVectorToHemiOctahedron(normalize(dir));

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
	
    vec2 position = vPosition.xy * uniforms.radius;

	vec4 up, right;

#ifdef INTERPOLATION
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

    vec4 modelPosition = mMatrix * vec4((normalize(up.xyz) * position.y
        + normalize(right.xyz) * position.x) + uniforms.center.xyz, 1.0);

#ifdef PIXEL_DEPTH_OFFSET
	instanceScale = vec3(
		length(mMatrix[0]),
		length(mMatrix[1]),
		length(mMatrix[2])
	);
	modelPositionVS = vec3(modelPosition);
#endif

    gl_Position =  pushConstants.lightSpaceMatrix * modelPosition;

}
