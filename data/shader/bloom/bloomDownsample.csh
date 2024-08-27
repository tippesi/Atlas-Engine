layout (local_size_x = 8, local_size_y = 8) in;

#include <common/sample.hsh>
#include <common/tiling.hsh>

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

layout(push_constant) uniform constants {
    int mipLevel;
} pushConstants;

vec3 Sample(vec2 texCoord) {

    return textureLod(textureIn, texCoord, float(pushConstants.mipLevel)).rgb;

}

void main() {

    ivec2 size = imageSize(textureOut);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x &&
        coord.y < size.y) {

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

        vec3 filtered = vec3(0.0);

        filtered += 0.125 * (outer00 + outer10 + outer01 + outer11) * 0.25;
        filtered += 0.125 * (outer10 + outer20 + outer11 + outer21) * 0.25;
        filtered += 0.125 * (outer01 + outer11 + outer02 + outer12) * 0.25;
        filtered += 0.125 * (outer11 + outer21 + outer12 + outer22) * 0.25;
        filtered += 0.5 * (inner00 + inner10 + inner01 + inner11) * 0.25;
        
        imageStore(textureOut, coord, vec4(filtered, 1.0));
        
    }

}