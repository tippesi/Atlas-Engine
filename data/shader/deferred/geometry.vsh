layout(location=0)in vec3 vPosition;
layout(location=1)in vec3 vNormal;
layout(location=2)in vec2 vTexCoord;
layout(location=4)in mat4 mMatrix;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3)in vec4 vTangent;
#endif
#ifdef ANIMATION
layout(location=4)in uvec4 vBoneIDs;
layout(location=5)in vec4 vBoneWeights;
#endif

out vec2 fTexCoord;

out vec3 ndcCurrent;
out vec3 ndcLast;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
out mat3 toTangentSpace;
#endif

out vec3 fNormal;
out vec3 fPosition;

#ifdef ANIMATION
layout (std140) uniform AnimationUBO {
    mat4 boneMatrices[256];
};
#endif
uniform mat4 pMatrix;
uniform mat4 vMatrix;

uniform bool invertUVs;

uniform mat4 pvMatrixLast;
uniform mat4 pvMatrixCurrent;

void main() {

	fTexCoord = invertUVs ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
	
#ifdef ANIMATION
	mat4 boneTransform = boneMatrices[vBoneIDs[0]] * vBoneWeights.x;
    boneTransform += boneMatrices[vBoneIDs[1]] * vBoneWeights.y;
    boneTransform += boneMatrices[vBoneIDs[2]] * vBoneWeights.z;
    boneTransform += boneMatrices[vBoneIDs[3]] * vBoneWeights.w;
	
	mat4 mvMatrix = vMatrix * mMatrix * boneTransform;
#else
	mat4 mvMatrix = vMatrix * mMatrix;
#endif   
	vec4 positionToCamera = mvMatrix * vec4(vPosition, 1.0);
	fPosition = positionToCamera.xyz;
	
	gl_Position = pMatrix * positionToCamera;

	ndcCurrent = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
	vec4 last = pvMatrixLast * mMatrix * vec4(vPosition, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
#ifdef WORLD_TRANSFORM
	mvMatrix = mMatrix;
#endif
	
	fNormal = mat3(mvMatrix) * vNormal;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 norm = normalize(fNormal);
	float correctionFactor = vTangent.w * (invertUVs ? -1.0 : 1.0);
    vec3 tang = normalize(mat3(mvMatrix) * vTangent.xyz);
	
	vec3 bitang = normalize(correctionFactor * cross(tang, norm));

	toTangentSpace = mat3(tang, bitang, norm);
#endif

#if defined(REFLECTION)
	fPosition = vec3(positionToCamera);
#endif
	
}