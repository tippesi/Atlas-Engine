layout(location=0)in vec3 vPosition;
layout(location=1)in vec2 vTexCoord;
#ifndef FLAT
layout(location=2)in vec3 vNormal;
#endif
#ifdef NORMALMAPPING
layout(location=3)in vec3 vTangent;
#endif
#ifdef ANIMATION
layout(location=4)in uvec4 vBoneIDs;
layout(location=5)in vec4 vBoneWeights;
#endif
#ifdef INSTANCING
layout(location=6)in mat4 mMatrix;
#endif

out vec2 fTexCoord;
out vec3 fPosition;

#ifdef NORMALMAPPING
out mat3 toTangentSpace;
#endif

#ifdef SHADOWS
out vec3 modelPosition;
#endif

#ifdef ANIMATION
layout (std140) uniform AnimationUBO {
    mat4 boneMatrices[256];
};
#endif

#ifndef FLAT
out vec3 fNormal;
#endif

uniform mat4 pMatrix;
uniform mat4 vMatrix;
#ifndef INSTANCING
uniform mat4 mMatrix;
#endif

void main() {

	fTexCoord = vTexCoord;
	
#ifdef ANIMATION
	mat4 boneTransform = boneMatrices[vBoneIDs[0]] * vBoneWeights.x;
    boneTransform += boneMatrices[vBoneIDs[1]] * vBoneWeights.y;
    boneTransform += boneMatrices[vBoneIDs[2]] * vBoneWeights.z;
    boneTransform += boneMatrices[vBoneIDs[3]] * vBoneWeights.w;
	
	mat4 bmMatrix = mMatrix * boneTransform;
#else
	mat4 bmMatrix = mMatrix;
#endif

	mat4 mvMatrix = vMatrix * bmMatrix;
   
#ifdef SHADOWS
	vec4 positionToCamera = bmMatrix * vec4(vPosition, 1.0f);
	modelPosition = positionToCamera.xyz;
	positionToCamera = vMatrix * positionToCamera;	
#else
	vec4 positionToCamera = mvMatrix * vec4(vPosition, 1.0f);
#endif
	
	fPosition = positionToCamera.xyz;
	
	gl_Position = pMatrix * positionToCamera;
	
#ifndef FLAT
	fNormal = (mvMatrix * vec4(vNormal, 0.0f)).xyz;
#endif

#if defined(NORMALMAPPING) && !defined(FLAT)
    vec3 norm = normalize(fNormal);
    vec3 tang = normalize((mvMatrix * vec4(vTangent, 0.0f)).xyz);
	
	vec3 bitang = cross(tang, norm);   

	toTangentSpace = mat3(tang, bitang, norm);
#endif
	
}