#include <../common/random.hsh>
#include <../common/normalencode.hsh>
#include <../globals.hsh>
#include <texture.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec2 normalFS;
layout (location = 2) out vec2 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out vec3 emissiveFS;
layout (location = 5) out uint materialIdxFS;
layout (location = 6) out vec2 velocityFS;

layout(location=0) in vec3 positionVS;
layout(location=1) in vec3 normalVS;
#ifdef TEX_COORDS
layout(location=2) in vec2 texCoordVS;
#endif

layout(location=3) in vec3 ndcCurrentVS;
layout(location=4) in vec3 ndcLastVS;

#ifdef VERTEX_COLORS
layout(location=5) in vec4 vertexColorsVS;
#endif

layout(location=6) in float normalInversionVS;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=7) in mat3 TBN;
#endif

layout(push_constant) uniform constants {
    uint vegetation;
    uint invertUVs;
    uint twoSided;
    uint staticMesh;
    uint materialIdx;
    float normalScale;
    float displacementScale;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
    uint baseColorTextureIdx;
    uint opacityTextureIdx;
    uint normalTextureIdx;
    uint roughnessTextureIdx;
    uint metalnessTextureIdx;
    uint aoTextureIdx;
    uint heightTextureIdx;
    uint emissiveTextureIdx;
} PushConstants;

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir) {
#ifdef HEIGHT_MAP
    float NdotV = max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0);
    const float minLayers = 4.0;
    const float maxLayers = 16.0;
    float numLayers = mix(minLayers, maxLayers, 1.0 - NdotV);
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * PushConstants.displacementScale;
    vec2 deltaTexCoords = P / numLayers;
    vec2  currentTexCoords = texCoords;
    
    vec2 ddx = dFdx(texCoords);
    vec2 ddy = dFdy(texCoords);
    
    float currentDepthMapValue = 1.0 - SampleHeight(currentTexCoords, PushConstants.heightTextureIdx, ddx, ddy).r;
    
    while(currentLayerDepth < currentDepthMapValue) {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1.0 - SampleHeight(currentTexCoords, PushConstants.heightTextureIdx, ddx, ddy).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1.0 - SampleHeight(prevTexCoords, PushConstants.heightTextureIdx, ddx, ddy).r - currentLayerDepth + layerDepth;
     
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = mix(currentTexCoords, prevTexCoords, weight);

    return finalTexCoords;
#else
    return vec2(1.0);
#endif

}

void main() {

#ifdef TEX_COORDS
    vec2 texCoords = texCoordVS;
#endif
    
    // Check if usage is valid (otherwise texCoords won't be used)
#if defined(HEIGHT_MAP) && (defined(BASE_COLOR_MAP) || defined(NORMAL_MAP) || defined(ROUGHNESS_MAP) || defined(METALNESS_MAP) || defined(AO_MAP) || defined(EMISSIVE_MAP)) 
    vec3 viewDir = normalize(transpose(TBN) * -positionVS);
    texCoords = ParallaxMapping(texCoords, viewDir);
#endif

    baseColorFS = vec3(1.0);

#if (defined(OPACITY_MAP) || defined(VERTEX_COLORS))
    float opacity = 1.0;
#ifdef OPACITY_MAP
    opacity *= SampleOpacity(texCoords, PushConstants.opacityTextureIdx, globalData.mipLodBias);
#endif
#ifdef VERTEX_COLORS
    // opacity *= vertexColorsVS.a;
#endif
    if (opacity < 0.2)
        discard;
#endif

#ifdef BASE_COLOR_MAP
    vec3 textureColor = SampleBaseColor(texCoords, PushConstants.baseColorTextureIdx, globalData.mipLodBias);
    baseColorFS *= textureColor.rgb;
#endif

#ifdef EMISSIVE_MAP
    vec3 emissiveColor = SampleEmissive(texCoords, PushConstants.emissiveTextureIdx, globalData.mipLodBias);
    emissiveFS = emissiveColor.rgb;
#endif

#ifdef VERTEX_COLORS
    baseColorFS *= vertexColorsVS.rgb;
#endif

    vec3 geometryNormal = normalize(normalVS);

#ifdef NORMAL_MAP
    vec3 normalColor = SampleNormal(texCoords, PushConstants.normalTextureIdx, globalData.mipLodBias);
    vec3 normal = mix(geometryNormal, normalize(TBN * normalColor), PushConstants.normalScale);
    // We want the normal always to face the camera for two sided materials
    geometryNormal *= PushConstants.twoSided > 0 ? gl_FrontFacing ? 1.0 : -1.0 : 1.0;
    normal *= dot(geometryNormal, normal) < 0.0 ? -1.0 : 1.0;
    normal *= normalInversionVS;

    normalFS = EncodeNormal(normal);
#else
    // We want the normal always to face the camera for two sided materials
    geometryNormal *= PushConstants.twoSided > 0 ? gl_FrontFacing ? 1.0 : -1.0 : 1.0;
#endif

    geometryNormal *= normalInversionVS;
    
    geometryNormalFS = EncodeNormal(geometryNormal);

    float roughnessFactor = 1.0;
    float metalnessFactor = 1.0;
    float aoFactor = 1.0;

#ifdef ROUGHNESS_MAP
    roughnessFactor *= SampleRoughness(texCoords, PushConstants.roughnessTextureIdx, globalData.mipLodBias);
    roughnessMetalnessAoFS.r = roughnessFactor;
#endif
#ifdef METALNESS_MAP
    metalnessFactor *= SampleMetalness(texCoords, PushConstants.metalnessTextureIdx, globalData.mipLodBias);
    roughnessMetalnessAoFS.g = metalnessFactor;
#endif
#ifdef AO_MAP
    aoFactor *= SampleAo(texCoords, PushConstants.aoTextureIdx, globalData.mipLodBias);
    roughnessMetalnessAoFS.b = aoFactor;
#endif

    // Calculate velocity
    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = PushConstants.materialIdx;
    
}