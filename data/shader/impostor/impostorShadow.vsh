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
layout(location=8) flat out mat4 instanceMatrix;
#endif

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout(std430, set = 1, binding = 2) buffer Matrices {
    mat3x4 matrices[];
};

layout (std430, set = 3, binding = 1) buffer ViewPlanes {
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
	
	vec3 pos = vec3(mMatrix * vec4(uniforms.center.xyz, 1.0));
	vec3 dir = pushConstants.lightLocation.xyz - pos;
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

	vec4 up = pushConstants.lightUp;
	vec4 right = pushConstants.lightRight;

    vec4 modelPosition = vec4((normalize(up.xyz) * position.y
        + normalize(right.xyz) * position.x) + uniforms.center.xyz, 1.0);

#ifdef PIXEL_DEPTH_OFFSET
	instanceMatrix = pushConstants.lightSpaceMatrix * mMatrix;
	modelPositionVS = modelPosition.xyz;
#endif

    gl_Position =  pushConstants.lightSpaceMatrix * mMatrix * modelPosition;

}
