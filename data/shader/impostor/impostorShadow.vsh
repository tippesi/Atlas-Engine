#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/octahedron.hsh>

layout(location=0) in vec2 vPosition;
layout(location=1) in mat4 mMatrix;

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout (std430, binding = 1) buffer ViewPlanes {
	ViewPlane viewPlanes[];
};

out vec2 texCoordVS;
flat out int indexVS;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform vec3 cameraLocation;

uniform vec3 center;
uniform float radius;

uniform int views;

void main() {

    texCoordVS = 0.5 * vPosition + 0.5;
	
	vec3 pos = vec3(mMatrix * vec4(0.0, 0.0, 0.0, 1.0));
	vec3 dir = cameraLocation - pos;
    float frames = float(views);

	vec2 octahedron = UnitVectorToHemiOctahedron(normalize(dir));
	vec2 coord = round(octahedron * (frames - 1.0));
	
	indexVS = Flatten2D(ivec2(coord), ivec2(frames));
	
    vec2 position = vPosition.xy * radius;

    ViewPlane viewPlane = viewPlanes[indexVS];
    vec4 modelPosition = vec4((viewPlane.up.xyz * position.y
        + viewPlane.right.xyz * position.x) + center, 1.0);	

    gl_Position =  pMatrix * vMatrix * mMatrix * modelPosition;

}
