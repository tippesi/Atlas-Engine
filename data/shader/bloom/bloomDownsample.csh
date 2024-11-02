layout (local_size_x = 16, local_size_y = 16) in;

#include <../common/utility.hsh>
#include <../common/types.hsh>
#include <../common/flatten.hsh>

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

AeF16 Luma(AeF16x3 color) {

    const AeF16x3 luma = AeF16x3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

AeF16x3 Prefilter(AeF16x3 color) {

    AeF16 brightness = Luma(color);
	AeF16 contribution = max(AeF16(0.0), brightness - AeF16(pushConstants.threshold));
    contribution /= max(brightness, AeF16(0.001));
    return color * contribution;

}


AeF16x3 Sample(vec2 texCoord) {

    if (pushConstants.mipLevel == 0) {
        return Prefilter(AeF16x3(textureLod(textureIn, texCoord, float(pushConstants.mipLevel)).rgb));
    }
    else {
        return AeF16x3(textureLod(textureIn, texCoord, float(pushConstants.mipLevel)).rgb);
    }

}

void main() {

    ivec2 size = imageSize(textureOut);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x && coord.y < size.y) {

        // Lower mip tex coord 
        vec2 texCoord = (coord + 0.5) / size;
        // Upper mip texel size
        vec2 texelSize = 1.0 / vec2(textureSize(textureIn, pushConstants.mipLevel));

        // We always sample at pixel border, not centers
        AeF16x3 outer00 = Sample(texCoord + vec2(-2.0 * texelSize.x, -2.0 * texelSize.y));
        AeF16x3 outer10 = Sample(texCoord + vec2(0.0, -2.0 * texelSize.y));
        AeF16x3 outer20 = Sample(texCoord + vec2(2.0 * texelSize.x, -2.0 * texelSize.y));

        AeF16x3 outer01 = Sample(texCoord + vec2(-2.0 * texelSize.x, 0.0));
        AeF16x3 outer11 = Sample(texCoord + vec2(0.0, 0.0));
        AeF16x3 outer21 = Sample(texCoord + vec2(2.0 * texelSize.x, 0.0));

        AeF16x3 outer02 = Sample(texCoord + vec2(-2.0 * texelSize.x, 2.0 * texelSize.y));
        AeF16x3 outer12 = Sample(texCoord + vec2(0.0, 2.0 * texelSize.y));
        AeF16x3 outer22 = Sample(texCoord + vec2(2.0 * texelSize.x, 2.0 * texelSize.y));

        AeF16x3 inner00 = Sample(texCoord + vec2(-texelSize.x, -texelSize.y));
        AeF16x3 inner10 = Sample(texCoord + vec2(texelSize.x, -texelSize.y));
        AeF16x3 inner01 = Sample(texCoord + vec2(-texelSize.x, texelSize.y));
        AeF16x3 inner11 = Sample(texCoord + vec2(texelSize.x, texelSize.y));

        AeF16x3 outerGroup0 = AeF16(0.125 * 0.25) * (outer00 + outer10 + outer01 + outer11);
        AeF16x3 outerGroup1 = AeF16(0.125 * 0.25) * (outer10 + outer20 + outer11 + outer21);
        AeF16x3 outerGroup2 = AeF16(0.125 * 0.25) * (outer01 + outer11 + outer02 + outer12);
        AeF16x3 outerGroup3 = AeF16(0.125 * 0.25) * (outer11 + outer21 + outer12 + outer22);
        AeF16x3 innerGroup = AeF16(0.5 * 0.25) * (inner00 + inner10 + inner01 + inner11);

        if (pushConstants.mipLevel == 0) {
            AeF16 outerGroup0Weight = (AeF16(1.0) / (AeF16(1.0) + Luma(outerGroup0)));
            AeF16 outerGroup1Weight = (AeF16(1.0) / (AeF16(1.0) + Luma(outerGroup1)));
            AeF16 outerGroup2Weight = (AeF16(1.0) / (AeF16(1.0) + Luma(outerGroup2)));
            AeF16 outerGroup3Weight = (AeF16(1.0) / (AeF16(1.0) + Luma(outerGroup3)));
            AeF16 innerGroupWeight = (AeF16(1.0) / (AeF16(1.0) + Luma(innerGroup)));
            
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