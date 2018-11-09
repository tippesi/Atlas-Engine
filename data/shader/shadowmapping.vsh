layout(location=0)in vec3 vPosition;
#ifdef ALPHA
layout(location=1)in vec2 vTexCoord;
#endif
#ifdef ANIMATION
layout(location=4)in uvec4 vBoneIDs;
layout(location=5)in vec4 vBoneWeights;
#endif
#ifdef INSTANCING
layout(location=6)in mat4 mMatrix;
#endif

uniform mat4 lightSpaceMatrix;
#ifndef INSTANCING
uniform mat4 mMatrix;
#endif

#ifdef ANIMATION
#ifdef ANIMATION
layout (std140) uniform AnimationUBO {
    mat4 boneMatrices[256];
};
#endif
#endif

#ifdef ALPHA
out vec2 fTexCoord;
#endif

void main() {
	
#ifdef ALPHA
	fTexCoord = vTexCoord;
#endif
#ifdef ANIMATION
	mat4 boneTransform = boneMatrices[vBoneIDs[0]] * vBoneWeights.x;
    boneTransform += boneMatrices[vBoneIDs[1]] * vBoneWeights.y;
    boneTransform += boneMatrices[vBoneIDs[2]] * vBoneWeights.z;
    boneTransform += boneMatrices[vBoneIDs[3]] * vBoneWeights.w;
	
	mat4 matrix = mMatrix * boneTransform;
#else
	mat4 matrix = mMatrix;
#endif
	
	gl_Position = lightSpaceMatrix * matrix * vec4(vPosition, 1.0f); 
	
}