layout (local_size_x = 8, local_size_y = 8) in;

#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/compute.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/random.hsh>

layout (set = 3, binding = 0, r16f) writeonly uniform image2D textureOut;
layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D shadowMap;
layout(set = 3, binding = 3) uniform sampler2D randomTexture;

layout(set = 3, binding = 4) uniform UniformBuffer {
    float radius;
    int sampleCount;
    int frameCount;
} uniforms;

layout(set = 3, binding = 5) uniform SampleBuffer {
    vec4 data[64];
} samples;

void main() {

    ivec2 resolution = ivec2(imageSize(textureOut));

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    //ivec2 pixel = GetScanlineLayout(int(resolution.x), int(resolution.y), 8);
    if (pixel.x >= int(resolution.x) || pixel.y >= int(resolution.y)) return;

    vec2 texCoord = (vec2(pixel) + 0.5) / resolution;
	
	// tile noise texture over screen based on screen dimensions divided by noise size
	vec2 noiseScale = vec2(resolution.x / 4.0, resolution.y / 4.0);

	float depth = texelFetch(shadowMap, pixel, 0).r;
	
	// Early exit, also prevents halo
		
	vec3 fragPos = ConvertDepthToViewSpace(depth, texCoord);
    vec3 norm = 2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0;
    vec3 randomVec = vec3(2.0 * texelFetch(randomTexture, pixel % ivec2(4), 0).xy - 1.0, 0.0);
	
    //Create TBN matrix
    vec3 tang = normalize(randomVec - norm * dot(randomVec, norm));
    vec3 bitang = normalize(cross(norm, tang));
    mat3 TBN = mat3(tang, bitang, norm);
	
    //Calculate occlusion factor
    float occlusion = 0.0;
    float seed = float(uniforms.frameCount);
    for(uint i = 0; i < uniforms.sampleCount; i++) {
        float rnd = 2.0 * random(texCoord, seed);
        // get sample position
        vec3 ssaoSample = TBN * samples.data[i].xyz; // From tangent to view-space
        ssaoSample = fragPos + ssaoSample * uniforms.radius;
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(ssaoSample, 1.0);
        offset = globalData.pMatrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
		vec3 samplePos = ConvertDepthToViewSpace(textureLod(shadowMap, offset.xy, 0).r, offset.xy);
     
		float rangeCheck = abs(fragPos.z - samplePos.z) < uniforms.radius ? 1.0 : 0.0;
		float delta = samplePos.z - ssaoSample.z;
        occlusion += (delta > 0.0 ? 1.0 : 0.0) * rangeCheck;
		
    }
	
    float result = pow(1.0 - (occlusion / float(uniforms.sampleCount)), 2.0);
    result = depth == 1.0 ? 0.0 : result;
    imageStore(textureOut, pixel, vec4(result, 0.0, 0.0, 0.0));
   
}