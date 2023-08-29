layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 3, binding = 0, rgba16f) readonly uniform image2D displacementMap;
layout (set = 3, binding = 1, rgba16f) writeonly uniform image2D normalMap;
layout (set = 3, binding = 2, rgba16f) readonly uniform image2D historyMap;

layout(push_constant) uniform constants {
    int N;
    int L;
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

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    
    vec2 fCoord = vec2(coord);
    float fN = float(PushConstants.N);
    
    float tileSize = PushConstants.tiling / float(PushConstants.N);
    float invTileSize = 1.0 / tileSize;
    
    vec3 center = imageLoad(displacementMap, coord).grb;
    vec3 left = imageLoad(displacementMap, ivec2(mod(fCoord.x - 1.0, fN), coord.y)).grb;
    vec3 right = imageLoad(displacementMap, ivec2(mod(fCoord.x + 1.0, fN), coord.y)).grb;
    vec3 top = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y + 1.0, fN))).grb;
    vec3 bottom = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y - 1.0, fN))).grb;
    
    center = AdjustScale(center);
    left = AdjustScale(left);
    right = AdjustScale(right);
    top = AdjustScale(top);
    bottom = AdjustScale(bottom);

    float history = imageLoad(historyMap, coord).a;

    // Calculate jacobian
    vec2 Dx = (right.xz - left.xz) * invTileSize;
    vec2 Dy = (top.xz - bottom.xz) * invTileSize;
    float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;

    float fold = -clamp(J, -1.0, 1.0) + PushConstants.foamOffset;

    /*
    float blend = fold > PushConstants.temporalThreshold ? 0.0 : 0.0;
    fold = mix(fold, history, blend);
    */

    vec2 gradient = vec2(left.y - right.y, bottom.y - top.y);
    
    imageStore(normalMap, coord, vec4(gradient, 0.0, fold));

}