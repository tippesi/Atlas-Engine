#include <../common/random.hsh>
#include <../common/normalencode.hsh>
#include <../globals.hsh>
#include <../common/opacity.hsh>
#include <texture.hsh>
#include <geometry.hsh>

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

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=7) in mat3 TBN;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
layout(location=10) in vec3 objectPositionVS;
layout(location=11) in vec3 instancePositionVS;
layout(location=12) in vec3 objectNormalVS;
#endif

layout(location=13) flat in uint lodVS;
#ifdef DRAW_DATA
layout(location=14) flat in uint drawIdxVS;
#endif

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
    vec2 P = viewDir.xy / viewDir.z * DrawValue(displacementScale);
    vec2 deltaTexCoords = P / numLayers;
    vec2  currentTexCoords = texCoords;
    
    vec2 ddx = dFdx(texCoords);
    vec2 ddy = dFdy(texCoords);
    
    float currentDepthMapValue = 1.0 - SampleHeight(currentTexCoords, DrawValue(heightTextureIdx), ddx, ddy).r;
    
    while(currentLayerDepth < currentDepthMapValue) {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1.0 - SampleHeight(currentTexCoords, DrawValue(heightTextureIdx), ddx, ddy).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1.0 - SampleHeight(prevTexCoords, DrawValue(heightTextureIdx), ddx, ddy).r - currentLayerDepth + layerDepth;
     
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = mix(currentTexCoords, prevTexCoords, weight);

    return finalTexCoords;
#else
    return vec2(1.0);
#endif

}

void main() {

#ifdef DRAW_DATA
    drawIdx = drawIdxVS;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
    vec2 graphTexCoord = vec2(0.0);
    vec4 graphColor = vec4(1.0);
#ifdef TEX_COORDS
    graphTexCoord = texCoordVS;
#endif
#ifdef VERTEX_COLORS
    graphColor = vertexColorsVS;
#endif

    float graphFrontFacing = gl_FrontFacing ? 1.0 : -1.0;
    vec3 graphGeometryNormal = normalize(normalVS) * (DrawValue(twoSided) > 0 ? graphFrontFacing : 1.0);
    mat3 graphTangentFrame = mat3(1.0);
#if defined(MATERIAL_GRAPH_WRITES_NORMAL) || defined(MATERIAL_GRAPH_USES_TANGENTS)
#ifdef NORMAL_MAP
    graphTangentFrame = TBN;
#else
    graphTangentFrame = MaterialGraphCotangentFrame(graphGeometryNormal, positionVS, graphTexCoord);
#endif
#endif

    MaterialSurfaceInput surfaceInput = CreateMaterialSurfaceInput(objectPositionVS, normalize(objectNormalVS),
        vec3(globalData.ivMatrix * vec4(positionVS, 1.0)), normalize(mat3(globalData.ivMatrix) * graphGeometryNormal),
        instancePositionVS, graphTangentFrame, graphTexCoord, 0.5 * ndcCurrentVS.xy / ndcCurrentVS.z + 0.5,
        graphColor, graphFrontFacing, PushConstants.materialGraphOffset);
    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceInput);

#ifdef MATERIAL_GRAPH_OPACITY
    if (surface.opacity < opacityCutoff)
        discard;
#endif

    baseColorFS = surface.baseColor;
    emissiveFS = surface.emissive;
    roughnessMetalnessAoFS = vec3(surface.roughness, surface.metalness, surface.ao);
    geometryNormalFS = EncodeNormal(graphGeometryNormal);

#ifdef MATERIAL_GRAPH_WRITES_NORMAL
    vec3 graphNormal = normalize(graphTangentFrame * surface.normal);
    graphNormal *= dot(graphGeometryNormal, graphNormal) < 0.0 ? -1.0 : 1.0;
    normalFS = EncodeNormal(graphNormal);
#else
    normalFS = geometryNormalFS;
#endif
#else
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
    opacity *= SampleOpacity(texCoords, DrawValue(opacityTextureIdx), globalData.mipLodBias);
#endif
#ifdef VERTEX_COLORS
    // opacity *= vertexColorsVS.a;
#endif
    if (opacity < opacityCutoff)
        discard;
#endif

#ifdef BASE_COLOR_MAP
    vec3 textureColor = SampleBaseColor(texCoords, DrawValue(baseColorTextureIdx), globalData.mipLodBias);
    baseColorFS *= textureColor.rgb;
#endif

#ifdef EMISSIVE_MAP
    vec3 emissiveColor = SampleEmissive(texCoords, DrawValue(emissiveTextureIdx), globalData.mipLodBias);
    emissiveFS = emissiveColor.rgb;
#endif

#ifdef VERTEX_COLORS
    baseColorFS *= vertexColorsVS.rgb;
#endif

    vec3 geometryNormal = normalize(normalVS);

#ifdef NORMAL_MAP
    vec3 normalColor = SampleNormal(texCoords, DrawValue(normalTextureIdx), globalData.mipLodBias);
    vec3 normal = mix(geometryNormal, normalize(TBN * normalColor), DrawValue(normalScale));
    // We want the normal always to face the camera for two sided materials
    geometryNormal *= DrawValue(twoSided) > 0 ? gl_FrontFacing ? 1.0 : -1.0 : 1.0;
    normal *= dot(geometryNormal, normal) < 0.0 ? -1.0 : 1.0;

    normalFS = EncodeNormal(normal);
#else
    // We want the normal always to face the camera for two sided materials
    geometryNormal *= DrawValue(twoSided) > 0 ? gl_FrontFacing ? 1.0 : -1.0 : 1.0;
#endif

    
    geometryNormalFS = EncodeNormal(geometryNormal);
#ifndef NORMAL_MAP
    // Without a normal map the geometry normal is the shading normal
    normalFS = geometryNormalFS;
#endif

    float roughnessFactor = 1.0;
    float metalnessFactor = 1.0;
    float aoFactor = 1.0;

#ifdef ROUGHNESS_MAP
    roughnessFactor *= SampleRoughness(texCoords, DrawValue(roughnessTextureIdx), globalData.mipLodBias);
    roughnessMetalnessAoFS.r = roughnessFactor;
#endif
#ifdef METALNESS_MAP
    metalnessFactor *= SampleMetalness(texCoords, DrawValue(metalnessTextureIdx), globalData.mipLodBias);
    roughnessMetalnessAoFS.g = metalnessFactor;
#endif
#ifdef AO_MAP
    aoFactor *= SampleAo(texCoords, DrawValue(aoTextureIdx), globalData.mipLodBias);
    roughnessMetalnessAoFS.b = aoFactor;
#endif
#endif

    // Calculate velocity
    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = DrawValue(materialIdx);

    if (globalData.debugMeshLods > 0u) {
        const vec3 lodColors[5] = vec3[](vec3(1.0), vec3(0.1, 0.9, 0.1), vec3(0.1, 0.4, 1.0), vec3(1.0, 0.8, 0.1),
            vec3(1.0, 0.1, 0.1));
        baseColorFS = lodColors[min(lodVS, 4u)] * (0.5 + 0.5 * dot(baseColorFS, vec3(0.333)));
    }

}
