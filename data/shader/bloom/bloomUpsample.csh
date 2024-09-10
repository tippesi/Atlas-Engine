layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

layout(push_constant) uniform constants {
    int additive;
    int mipLevel;
    float filterSize;
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
        vec2 texelSize = 1.0 / textureSize(textureIn, pushConstants.mipLevel);
        vec2 filterSize = vec2(pushConstants.filterSize);
        texelSize = max(filterSize, filterSize);

        // We always sample at pixel border, not centers
        vec3 filter00 = Sample(texCoord + vec2(-texelSize.x, -texelSize.y));
        vec3 filter10 = Sample(texCoord + vec2(0.0, -texelSize.y));
        vec3 filter20 = Sample(texCoord + vec2(texelSize.x, -texelSize.y));

        vec3 filter01 = Sample(texCoord + vec2(-texelSize.x, 0.0));
        vec3 filter11 = Sample(texCoord + vec2(0.0, 0.0));
        vec3 filter21 = Sample(texCoord + vec2(texelSize.x, 0.0));

        vec3 filter02 = Sample(texCoord + vec2(-texelSize.x, texelSize.y));
        vec3 filter12 = Sample(texCoord + vec2(0.0, texelSize.y));
        vec3 filter22 = Sample(texCoord + vec2(texelSize.x, texelSize.y));

        vec3 filtered = vec3(0.0);

        filtered += 4.0 * filter11;
        filtered += 2.0 * (filter10 + filter01 + filter21 + filter12);
        filtered += 1.0 * (filter00 + filter20 + filter02 + filter22);
        filtered /= 16.0;
        
        if (pushConstants.additive > 0) {
            imageStore(textureOut, coord, vec4(filtered + filter11, 1.0));
        }
        else {
            imageStore(textureOut, coord, vec4(filtered, 1.0));
        }
        
    }

}