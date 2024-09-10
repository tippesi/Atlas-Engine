layout (local_size_x = 16, local_size_y = 16) in;

#include <../common/utility.hsh>
#include <../common/flatten.hsh>

//#define NO_SHARED

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

layout(push_constant) uniform constants {
    int mipLevel;
    float threshold;
} pushConstants;

const uint supportSize = 2;
const uint sharedDataSize = (2u * gl_WorkGroupSize.x + 2u * supportSize) * (2u * gl_WorkGroupSize.y + 2u * supportSize);
const ivec2 unflattenedSharedDataSize = 2 * ivec2(gl_WorkGroupSize) + 2 * int(supportSize);

shared vec3 sharedMemory[sharedDataSize];

const ivec2 pixelOffsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

vec3 Prefilter(vec3 color) {

    float brightness = Luma(color);
	float contribution = max(0.0, brightness - pushConstants.threshold);
    contribution /= max(brightness, 0.00001);
    return color * contribution;

}

void LoadGroupSharedData() {

    ivec2 resolution = textureSize(textureIn, pushConstants.mipLevel);
    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + 2 * workGroupOffset - ivec2(supportSize);

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 color = texelFetch(textureIn, texel, pushConstants.mipLevel).rgb;
        if (pushConstants.mipLevel == 0) {
            color = Prefilter(color);
        }

        sharedMemory[i] = color;
    }

    barrier();

} 

vec3 Sample(vec2 texCoord) {

#ifdef NO_SHARED
    if (pushConstants.mipLevel == 0) {
        return Prefilter(textureLod(textureIn, texCoord, float(pushConstants.mipLevel)).rgb);
    }
    else {
        return textureLod(textureIn, texCoord, float(pushConstants.mipLevel)).rgb;
    }
#else
    const ivec2 groupOffset = 2 * (ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize)) - ivec2(supportSize);

    vec2 pixel = texCoord * textureSize(textureIn, pushConstants.mipLevel);
    pixel -= 0.5;

    float x = fract(pixel.x);
    float y = fract(pixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    vec3 color = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(pixel) + pixelOffsets[i];
        offsetPixel -= groupOffset;
        offsetPixel = clamp(offsetPixel, ivec2(0), unflattenedSharedDataSize - ivec2(1));
        int sharedMemoryIdx = Flatten2D(offsetPixel, unflattenedSharedDataSize);
        color += weights[i] * sharedMemory[sharedMemoryIdx];
    }
    return color;
#endif

}

void main() {

    LoadGroupSharedData();

    ivec2 size = imageSize(textureOut);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x && coord.y < size.y) {

        // Lower mip tex coord 
        vec2 texCoord = (coord + 0.5) / size;
        // Upper mip texel size
        vec2 texelSize = 1.0 / vec2(textureSize(textureIn, pushConstants.mipLevel));

        // We always sample at pixel border, not centers
        vec3 outer00 = Sample(texCoord + vec2(-2.0 * texelSize.x, -2.0 * texelSize.y));
        vec3 outer10 = Sample(texCoord + vec2(0.0, -2.0 * texelSize.y));
        vec3 outer20 = Sample(texCoord + vec2(2.0 * texelSize.x, -2.0 * texelSize.y));

        vec3 outer01 = Sample(texCoord + vec2(-2.0 * texelSize.x, 0.0));
        vec3 outer11 = Sample(texCoord + vec2(0.0, 0.0));
        vec3 outer21 = Sample(texCoord + vec2(2.0 * texelSize.x, 0.0));

        vec3 outer02 = Sample(texCoord + vec2(-2.0 * texelSize.x, 2.0 * texelSize.y));
        vec3 outer12 = Sample(texCoord + vec2(0.0, 2.0 * texelSize.y));
        vec3 outer22 = Sample(texCoord + vec2(2.0 * texelSize.x, 2.0 * texelSize.y));

        vec3 inner00 = Sample(texCoord + vec2(-texelSize.x, -texelSize.y));
        vec3 inner10 = Sample(texCoord + vec2(texelSize.x, -texelSize.y));
        vec3 inner01 = Sample(texCoord + vec2(-texelSize.x, texelSize.y));
        vec3 inner11 = Sample(texCoord + vec2(texelSize.x, texelSize.y));

        vec3 outerGroup0 = 0.125 * (outer00 + outer10 + outer01 + outer11) * 0.25;
        vec3 outerGroup1 = 0.125 * (outer10 + outer20 + outer11 + outer21) * 0.25;
        vec3 outerGroup2 = 0.125 * (outer01 + outer11 + outer02 + outer12) * 0.25;
        vec3 outerGroup3 = 0.125 * (outer11 + outer21 + outer12 + outer22) * 0.25;
        vec3 innerGroup = 0.5 * (inner00 + inner10 + inner01 + inner11) * 0.25;

        if (pushConstants.mipLevel == 0) {
            float outerGroup0Weight = (1.0 / (1.0 + Luma(outerGroup0)));
            float outerGroup1Weight = (1.0 / (1.0 + Luma(outerGroup1)));
            float outerGroup2Weight = (1.0 / (1.0 + Luma(outerGroup2)));
            float outerGroup3Weight = (1.0 / (1.0 + Luma(outerGroup3)));
            float innerGroupWeight = (1.0 / (1.0 + Luma(innerGroup)));
            
            outerGroup0 *= outerGroup0Weight;
            outerGroup1 *= outerGroup1Weight;
            outerGroup2 *= outerGroup2Weight;
            outerGroup3 *= outerGroup3Weight;
            innerGroup *= innerGroupWeight;
        }

        vec3 filtered = outerGroup0 + outerGroup1 + outerGroup2 + outerGroup3 + innerGroup;

        imageStore(textureOut, coord, vec4(filtered, 1.0));
        
    }

}