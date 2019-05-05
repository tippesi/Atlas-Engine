#include "convert"

out float fragColor;

in vec2 fTexCoord;

layout(binding = 0) uniform sampler2D randomTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D shadowMap;
uniform vec3 ssaoSamples[64];
uniform vec2 framebufferResolution;

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
uniform int ssaoNumSamples;
uniform float ssaoRadius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

void main() {
	
	// tile noise texture over screen based on screen dimensions divided by noise size
	vec2 noiseScale = vec2(framebufferResolution.x/4.0f, framebufferResolution.y/4.0f); 
   
	float depth = texture(shadowMap, fTexCoord).r;
	
	// Early exit, also prevents halo
	if(depth == 1.0f) {
		fragColor = 1.0f;
		return;
	}
		
	vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);
    vec3 norm = 2.0f * texture(textures[0], fTexCoord).rgb - 1.0f;
    vec3 randomVec = vec3(2.0f * texture(textures[1], fTexCoord * noiseScale).xy - 1.0f, 0.0f);
	
    //Create TBN matrix
    vec3 tang = normalize(randomVec - norm * dot(randomVec, norm));
    vec3 bitang = cross(norm, tang);
    mat3 TBN = mat3(tang, bitang, norm);
	
    //Calculate occlusion factor
    float occlusion = 0.0f;
    for(int i = 0; i < ssaoNumSamples; i++) {
        // get sample position
        vec3 ssaoSample = TBN * ssaoSamples[i]; // From tangent to view-space
        ssaoSample = fragPos + ssaoSample * ssaoRadius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(ssaoSample, 1.0);
        offset = pMatrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
		float sampleDepth = ConvertDepthToViewSpaceDepth(texture(shadowMap, offset.xy).r);
     
        // range check & accumulate
        float rangeCheck = abs(fragPos.z - sampleDepth) < ssaoRadius ? 1.0 : 0.0;
		float delta = sampleDepth - ssaoSample.z;
        occlusion += (delta > 0.0f ? 1.0f : 0.0f) * rangeCheck;
		
    }
	
    fragColor = 1.0 - (occlusion / float(ssaoNumSamples));
   
}