#include <../common/utility.hsh>

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (set = 3, binding = 0, rgba16f) readonly uniform image2DArray displacementMap;
layout (set = 3, binding = 1, rgba16f) writeonly uniform image2DArray normalMap;
// layout (set = 3, binding = 2, rgba16f) readonly uniform image2D historyMap;

layout(push_constant) uniform constants {
    ivec4 L;
    vec4 tilingFactor;
    int N;
    float choppyScale;
    float displacementScale;
    float tiling;

    float temporalWeight;
    float temporalThreshold;

    float foamOffset;
} PushConstants;

vec3 AdjustScale(vec3 point) {

    return vec3(point.x * PushConstants.choppyScale,
        point.y * PushConstants.displacementScale,
        point.z * PushConstants.choppyScale);
        
}

void main() {

    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    
    vec2 fCoord = vec2(coord);
    float fN = float(PushConstants.N);
    
    float tilingFactor = PushConstants.tilingFactor[coord.z];
    float tileSize = (tilingFactor * PushConstants.tiling) / float(PushConstants.N);
    float invTileSize = 1.0 / tileSize;
    
    vec3 center = imageLoad(displacementMap, coord).grb;
    vec3 left = imageLoad(displacementMap, ivec3(mod(fCoord.x - 1.0, fN), coord.y, coord.z)).grb;
    vec3 right = imageLoad(displacementMap, ivec3(mod(fCoord.x + 1.0, fN), coord.y, coord.z)).grb;
    vec3 top = imageLoad(displacementMap, ivec3(coord.x, mod(fCoord.y + 1.0, fN), coord.z)).grb;
    vec3 bottom = imageLoad(displacementMap, ivec3(coord.x, mod(fCoord.y - 1.0, fN), coord.z)).grb;
    
    center = AdjustScale(center);
    left = AdjustScale(left);
    right = AdjustScale(right);
    top = AdjustScale(top);
    bottom = AdjustScale(bottom);

    // float history = imageLoad(historyMap, coord).a;

    // Calculate jacobian
    vec2 Dx = (right.xz - left.xz);
    vec2 Dy = (top.xz - bottom.xz);
    float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;

    float fold = max(0.0, -clamp(J, -1.0, 1.0) + PushConstants.foamOffset);

    /*
    float blend = fold > PushConstants.temporalThreshold ? 0.0 : 0.0;
    fold = mix(fold, history, blend);
    */   
    
    float width = right.x - left.x;
    float height = bottom.z - top.z;
    vec2 gradient = vec2(left.y - right.y, bottom.y - top.y) / (1.0 + abs(vec2(width, height)));
    
    imageStore(normalMap, coord, vec4(gradient, 0.0, fold));

}