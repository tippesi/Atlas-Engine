#include <../wind.hsh>

// New naming convention:
// Vertex attributes v[Name]
// Fragment attributes f[Name]
// Etc.
// Vertex stage out paramters [name]VS
// Fragment stage out paramters [name]FS
// Etc.
// Uniform names: camelCase (variables in general)

// Per vertex attributes
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoord;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3) in vec4 vTangent;
#endif

layout(std430, binding = 2) buffer CurrentMatrices {
	mat4 currentMatrices[];
};

layout(std430, binding = 3) buffer LastMatrices {
	mat4 lastMatrices[];
};

// Vertex out parameters
out vec3 positionVS;
out vec3 normalVS;
out vec2 texCoordVS;

out vec3 ndcCurrentVS;
out vec3 ndcLastVS;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
out mat3 TBN;
#endif

// Uniforms
uniform mat4 pMatrix;
uniform mat4 vMatrix;

uniform float time;
uniform float deltaTime;

uniform bool vegetation;
uniform bool invertUVs;
uniform bool twoSided;
uniform bool staticMesh;

uniform mat4 pvMatrixLast;
uniform mat4 pvMatrixCurrent;

// Functions
void main() {

	mat4 mMatrix = currentMatrices[gl_InstanceID];
	mat4 mMatrixLast = staticMesh ? mMatrix : lastMatrices[gl_InstanceID];

	texCoordVS = invertUVs ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
	
	mat4 mvMatrix = vMatrix * mMatrix;

	vec3 position = vPosition;
	vec3 lastPosition = vPosition;

	if (vegetation) {

		position = WindAnimation(vPosition, time, mMatrix[3].xyz);
		lastPosition = WindAnimation(vPosition, time - deltaTime, mMatrix[3].xyz);

	}

	vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
	positionVS = positionToCamera.xyz;
	
	gl_Position = pMatrix * positionToCamera;

	// Needed for velocity buffer calculation 
	ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last frames matrix
	vec4 last = pvMatrixLast * mMatrixLast * vec4(lastPosition, 1.0);
	ndcLastVS = vec3(last.xy, last.w);
	
#ifdef GENERATE_IMPOSTOR
	mvMatrix = mMatrix;
#endif
	
	normalVS = mat3(mvMatrix) * vNormal;	

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 normal = normalize(normalVS);
	float correctionFactor = vTangent.w * (invertUVs ? -1.0 : 1.0);
    vec3 tangent = normalize(mat3(mvMatrix) * vTangent.xyz);
	
	vec3 bitangent = normalize(correctionFactor * 
		cross(tangent, normal));

	TBN = mat3(tangent, bitangent, normal);
#endif
	
}