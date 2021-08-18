#include <../common/convert.hsh>

out float fragColor;

in vec2 fTexCoord;

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D shadowMap;
layout(binding = 2) uniform sampler2D randomTexture;

uniform vec3 samples[64];
uniform vec2 resolution;

uniform uint sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

void main() {
	
	// tile noise texture over screen based on screen dimensions divided by noise size
	vec2 noiseScale = vec2(resolution.x / 4.0, resolution.y / 4.0); 
   
	float depth = texture(shadowMap, fTexCoord).r;
	
	// Early exit, also prevents halo
	if(depth == 1.0) {
		fragColor = 1.0;
		return;
	}
		
	vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);
    vec3 norm = 2.0 * texture(normalTexture, fTexCoord).rgb - 1.0;
    vec3 randomVec = vec3(2.0 * texture(randomTexture, fTexCoord * noiseScale).xy - 1.0, 0.0);
	
    //Create TBN matrix
    vec3 tang = normalize(randomVec - norm * dot(randomVec, norm));
    vec3 bitang = cross(norm, tang);
    mat3 TBN = mat3(tang, bitang, norm);
	
    //Calculate occlusion factor
    float occlusion = 0.0;
    for(uint i = 0; i < sampleCount; i++) {
        // get sample position
        vec3 ssaoSample = TBN * samples[i]; // From tangent to view-space
        ssaoSample = fragPos + ssaoSample * radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(ssaoSample, 1.0);
        offset = pMatrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
		vec3 samplePos = ConvertDepthToViewSpace(texture(shadowMap, offset.xy).r, offset.xy);
     
        // range check & accumulate
        float rangeCheck = distance(fragPos, samplePos) < radius ? 1.0 : 0.0;
		float delta = samplePos.z - ssaoSample.z;
        occlusion += (delta > 0.0 ? 1.0 : 0.0) * rangeCheck;		
    }
	
    fragColor = 1.0 - (occlusion / float(sampleCount));
   
}