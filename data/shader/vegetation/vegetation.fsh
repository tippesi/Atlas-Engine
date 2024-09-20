#include <../globals.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec2 normalFS;
layout (location = 2) out vec2 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out vec3 emissiveFS;
layout (location = 5) out uint materialIdxFS;
layout (location = 6) out vec2 velocityFS;

#ifdef BASE_COLOR_MAP
layout(set = 3, binding = 0) uniform sampler2D baseColorMap;
#endif
#ifdef OPACITY_MAP
layout(set = 3, binding = 1) uniform sampler2D opacityMap;
#endif
#ifdef NORMAL_MAP
layout(set = 3, binding = 2) uniform sampler2D normalMap;
#endif
#ifdef ROUGHNESS_MAP
layout(set = 3, binding = 3) uniform sampler2D roughnessMap;
#endif
#ifdef METALNESS_MAP
layout(set = 3, binding = 4) uniform sampler2D metalnessMap;
#endif
#ifdef AO_MAP
layout(set = 3, binding = 5) uniform sampler2D aoMap;
#endif
#ifdef HEIGHT_MAP
layout(set = 3, binding = 6) uniform sampler2D heightMap;
#endif

#ifdef NORMAl_MAP
layout(location=0) in vec3 positionVS;
#endif
layout(location=1) in vec3 normalVS;
#ifdef TEX_COORDS
layout(location=2) in vec2 texCoordVS;
#endif

layout(location=3) in vec3 ndcCurrentVS;
layout(location=4) in vec3 ndcLastVS;

#ifdef VERTEX_COLORS
layout(location=5) in vec4 vertexColorsVS;
#endif

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=6) in mat3 TBN;
#endif

layout(push_constant) uniform constants {
    uint invertUVs;
    uint twoSided;
    uint materialIdx;
    float normalScale;
    float displacementScale;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
} pushConstants;

void main() {
    
    vec2 texCoords = texCoordVS;

#if (defined(OPACITY_MAP) || defined(VERTEX_COLORS))
    float opacity = 1.0;
#ifdef OPACITY_MAP
    opacity *= texture(opacityMap, texCoords, globalData.mipLodBias).r;
#endif
#ifdef VERTEX_COLORS
    opacity *= vertexColorsVS.a;
#endif
    if (opacity < 0.2)
        discard;
#endif

    baseColorFS = vec3(1.0);

#ifdef BASE_COLOR_MAP
    vec3 textureColor = texture(baseColorMap, texCoords, globalData.mipLodBias).rgb;
    baseColorFS *= textureColor.rgb;
#endif
#ifdef VERTEX_COLORS
    baseColorFS.rgb *= vertexColorsVS.rgb;
#endif

    vec3 geometryNormal = normalize(normalVS);
    geometryNormalFS = EncodeNormal(geometryNormal);

    float roughnessFactor = 1.0;
    float metalnessFactor = 1.0;
    float aoFactor = 1.0;

#ifdef ROUGHNESS_MAP
    roughnessFactor *= texture(roughnessMap, texCoords, globalData.mipLodBias).r;
    roughnessMetalnessAoFS.r = roughnessFactor;
#endif
#ifdef METALNESS_MAP
    metalnessFactor *= texture(metalnessMap, texCoords, globalData.mipLodBias).r;
    roughnessMetalnessAoFS.g = metalnessFactor;
#endif
#ifdef AO_MAP
    aoFactor *= texture(aoMap, texCoords, globalData.mipLodBias).r;
    roughnessMetalnessAoFS.b = aoFactor;
#endif

    // Calculate velocity
    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = pushConstants.materialIdx;
    
}