#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/octahedron.hsh>

layout(location=0) in vec2 vPosition;

layout(location=0) out vec2 texCoordVS;
layout(location=1) flat out int indexVS;

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout(std430, set = 1, binding = 2) buffer Matrices {
    mat4 matrices[];
};

layout (std430, set = 3, binding = 1) buffer ViewPlanes {
	ViewPlane viewPlanes[];
};

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;

    vec4 lightLocation;
    vec4 center;

    float radius;
    int views;
    float cutoff;
} PushConstants;

void main() {

    mat4 mMatrix = matrices[gl_InstanceIndex];

    texCoordVS = 0.5 * vPosition + 0.5;
	
	vec3 pos = vec3(mMatrix * vec4(0.0, 0.0, 0.0, 1.0));
	vec3 dir = PushConstants.lightLocation.xyz - pos;
    float frames = float(PushConstants.views);

	vec2 octahedron = UnitVectorToHemiOctahedron(normalize(dir));
	vec2 coord = round(octahedron * (frames - 1.0));
	
	indexVS = Flatten2D(ivec2(coord), ivec2(frames));
	
    vec2 position = vPosition.xy * PushConstants.radius;

    ViewPlane viewPlane = viewPlanes[indexVS];
    vec4 modelPosition = vec4((viewPlane.up.xyz * position.y
        + viewPlane.right.xyz * position.x) + PushConstants.center.xyz, 1.0);

    gl_Position =  PushConstants.lightSpaceMatrix * mMatrix * modelPosition;

}
