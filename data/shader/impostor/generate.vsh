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
#ifdef TEX_COORDS
layout(location=2) in vec2 vTexCoord;
#endif

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3) in vec4 vTangent;
#endif

// Vertex out parameters
layout(location=0) out vec3 positionVS;
layout(location=1) out vec3 normalVS;
#ifdef TEX_COORDS
layout(location=2) out vec2 texCoordVS;
#endif
layout(location=3) out float depthVS;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=5) out mat3 TBN;
#endif

layout(push_constant) uniform constants {
	mat4 vMatrix;
	vec4 baseColor;
	float roughness;
	float metalness;
	float ao;
	uint invertUVs;
	uint twoSided;
	float normalScale;
	float displacementScale;
	float distanceToPlaneCenter;
} PushConstants;

layout (set = 3, binding = 7, std140) uniform UniformBuffer {
	mat4 pMatrix;
} Uniforms;

// Functions
void main() {

#ifdef TEX_COORDS
	texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

	vec4 positionToCamera = PushConstants.vMatrix * vec4(vPosition, 1.0);
	positionVS = positionToCamera.xyz;
	
	gl_Position = Uniforms.pMatrix * PushConstants.vMatrix * vec4(vPosition, 1.0);

	// Want to render in world space
	mat4 mvMatrix = mat4(1.0);
	normalVS = mat3(mvMatrix) * vNormal;	

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 normal = normalize(normalVS);
	float correctionFactor = vTangent.w * (PushConstants.invertUVs > 0 ? -1.0 : 1.0);
    vec3 tangent = normalize(mat3(mvMatrix) * vTangent.xyz);
	
	vec3 bitangent = normalize(correctionFactor * 
		cross(tangent, normal));

	TBN = mat3(tangent, bitangent, normal);
#endif

	depthVS = positionToCamera.z + PushConstants.distanceToPlaneCenter;
	
}