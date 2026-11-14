#include <../common/utility.hsh>

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (set = 3, binding = 0, rgba16f) readonly uniform image2DArray displacementMap;
layout (set = 3, binding = 1, rgba16f) writeonly uniform image2DArray normalMap;
// layout (set = 3, binding = 2, rgba16f) readonly uniform image2D historyMap;

layout(push_constant) uniform constants {
    vec4 tilingFactor;
    vec4 spectrumScaling;
    int N;
    float choppyScale;
    float displacementScale;
    float tiling;

    float temporalWeight;
    float temporalThreshold;
} PushConstants;

vec3 AdjustScale(vec3 point) {

    return vec3(point.x * PushConstants.choppyScale,
        point.y * PushConstants.displacementScale,
        point.z * PushConstants.choppyScale) * PushConstants.spectrumScaling[gl_GlobalInvocationID.z];
        
}

void main() {

    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    
    vec2 fCoord = vec2(coord) + vec2(0.5);
    float fN = float(PushConstants.N);
    
    float tilingFactor = PushConstants.tilingFactor[coord.z];
    float tileSize = (tilingFactor * PushConstants.tiling) / float(PushConstants.N);
    float invTileSize = 1.0 / tileSize;
    
    vec3 center = imageLoad(displacementMap, coord).rgb;
    vec3 left = imageLoad(displacementMap, ivec3(mod(fCoord.x - 1.0, fN), coord.y, coord.z)).rgb;
    vec3 right = imageLoad(displacementMap, ivec3(mod(fCoord.x + 1.0, fN), coord.y, coord.z)).rgb;
    vec3 top = imageLoad(displacementMap, ivec3(coord.x, mod(fCoord.y + 1.0, fN), coord.z)).rgb;
    vec3 bottom = imageLoad(displacementMap, ivec3(coord.x, mod(fCoord.y - 1.0, fN), coord.z)).rgb;
    
    center = AdjustScale(center);
    left = AdjustScale(left);
    right = AdjustScale(right);
    top = AdjustScale(top);
    bottom = AdjustScale(bottom);

    // float history = imageLoad(historyMap, coord).a;

    // Calculate jacobian, it's 1 for undisturbed water and negative where the waves fold over
    vec2 Dx = 0.5 * invTileSize * (right.xz - left.xz);
    vec2 Dy = 0.5 * invTileSize * (top.xz - bottom.xz);
    float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;

    // The cascades are combined into an approximate jacobian of the whole surface when rendering
    float jacobianDeficit = 1.0 - J;
    
    float width = right.x - center.x;
    float height = top.z - center.z;
    vec2 gradient = vec2(right.y - center.y, top.y - center.y) / max(tileSize + vec2(width, height), tileSize * 0.01);
    
    // The squared slope is averaged by the mip maps, the rendering derives the roughness from it
    imageStore(normalMap, coord, vec4(-gradient, dot(gradient, gradient), jacobianDeficit));

}