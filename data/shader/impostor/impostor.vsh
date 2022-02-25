#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/octahedron.hsh>

layout(location=0) in vec2 vPosition;

struct ViewPlane {
    vec4 right;
    vec4 up;
};

layout (std430, binding = 1) buffer ViewPlanes {
	ViewPlane viewPlanes[];
};

layout(std430, binding = 2) buffer Matrices {
	mat4 matrices[];
};

out vec2 texCoordVS;
out vec3 ndcCurrentVS;
out vec3 ndcLastVS;

#ifdef INTERPOLATION
flat out int index0VS;
flat out int index1VS;
flat out int index2VS;

flat out float weight0VS;
flat out float weight1VS;
flat out float weight2VS;
#else
flat out int indexVS;
#endif

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform vec3 cameraLocation;

uniform vec3 center;
uniform float radius;

uniform int views;

uniform mat4 pvMatrixLast;

#ifdef INTERPOLATION
uniform vec3 cameraRight;
uniform vec3 cameraUp;
#endif

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
	
	mat4 mMatrix = matrices[gl_InstanceID];
    texCoordVS = 0.5 * vPosition + 0.5;
	
	vec3 pos = vec3(mMatrix * vec4(0.0, 0.0, 0.0, 1.0));
	vec3 dir = normalize(cameraLocation - pos);

    float frames = float(views);

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
	
    vec2 position = vPosition.xy * radius;

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
        + normalize(right.xyz) * position.x) + center, 1.0);	

    gl_Position =  pMatrix * vMatrix * mMatrix * modelPosition;

    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
    vec4 last = pvMatrixLast * mMatrix * modelPosition;
	ndcLastVS = vec3(last.xy, last.w);

}
