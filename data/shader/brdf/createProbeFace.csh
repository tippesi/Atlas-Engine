#include <../common/convert.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba16f) writeonly uniform imageCube textureOut;
layout (binding = 1, r16f) writeonly uniform imageCube depthOut;

layout (binding = 0) uniform sampler2D lightIn;
layout (binding = 1) uniform sampler2D depthIn;

uniform int faceIndex;

void main() {

    ivec2 size = textureSize(lightIn, 0);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
	
	if (coord.x < size.x &&
		coord.y < size.y) {

        // Needed for irradiance integration and sky visibility
        vec3 light = texelFetch(lightIn, coord, 0).rgb;
        float depth = texelFetch(depthIn, coord, 0).r;

        // Needed to calculate geometry visibility (irradiance volume)
        vec2 viewCoord = (vec2(coord) + vec2(0.5)) / vec2(size);
        float viewDepth = length(ConvertDepthToViewSpace(depth, viewCoord));

        imageStore(textureOut, ivec3(coord, faceIndex), vec4(light, depth));
        imageStore(depthOut, ivec3(coord, faceIndex), vec4(viewDepth, 0.0, 0.0, 0.0));

    }

}