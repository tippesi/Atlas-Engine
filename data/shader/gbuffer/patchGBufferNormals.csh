#include <../common/material.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, rg16f) uniform image2D normalImage;
layout (set = 3, binding = 1) uniform sampler2D geometryNormalTexture;
layout (set = 3, binding = 2) uniform usampler2D materialIdxTexture;

void main() {

    ivec2 size = imageSize(normalImage);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x &&
        coord.y < size.y) {

        vec2 normal = imageLoad(normalImage, coord).rg;
        vec2 geometryNormal = texelFetch(geometryNormalTexture, coord, 0).rg;

        uint materialIdx = texelFetch(materialIdxTexture, coord, 0).r;
        Material material = UnpackMaterial(materialIdx);

        normal = material.normalMap ? normal : geometryNormal;

        imageStore(normalImage, coord, vec4(normal, 0.0, 0.0));
        
    }

}