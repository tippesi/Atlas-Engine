#extension GL_EXT_nonuniform_qualifier : require

#include <terrainMaterial.hsh>
#include <../globals.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec2 normalFS;
layout (location = 2) out vec2 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out vec3 emissiveFS;
layout (location = 5) out uint materialIdxFS;
layout (location = 6) out vec2 velocityFS;

layout (set = 3, binding = 1) uniform sampler2D normalMap;
layout (set = 3, binding = 2) uniform usampler2D splatMap;
layout (set = 3, binding = 3) uniform sampler2DArray baseColorMaps;
layout (set = 3, binding = 4) uniform sampler2DArray roughnessMaps;
layout (set = 3, binding = 5) uniform sampler2DArray aoMaps;
layout (set = 3, binding = 6) uniform sampler2DArray normalMaps;

layout (set = 3, binding = 10) uniform sampler2D noiseMap;

layout (set = 3, binding = 8, std140) uniform UBO {
    TerrainMaterial materials[128];
} Materials;

layout (set = 3, binding = 9, std140) uniform UniformBuffer {
    vec4 frustumPlanes[6];

    float heightScale;
    float displacementDistance;

    float tessellationFactor;
    float tessellationSlope;
    float tessellationShift;
    float maxTessellationLevel;
} Uniforms;

layout(push_constant) uniform constants {
    float nodeSideLength;
    float tileScale;
    float patchSize;
    float normalTexelSize;

    float leftLoD;
    float topLoD;
    float rightLoD;
    float bottomLoD;

    vec2 nodeLocation;
} PushConstants;

layout(location=0) in vec2 materialTexCoords;
layout(location=1) in vec2 texCoords;
layout(location=2) in vec3 ndcCurrent;
layout(location=3) in vec3 ndcLast;

vec2 rotateUV(vec2 uv, float rotation)
{
    float mid = 0.5;
    return vec2(
        cos(rotation) * (uv.x - mid) + sin(rotation) * (uv.y - mid) + mid,
        cos(rotation) * (uv.y - mid) - sin(rotation) * (uv.x - mid) + mid
    );
}

vec3 hueShift(vec3 color, float hue) {
    const vec3 k = vec3(0.57735, 0.57735, 0.57735);
    float cosAngle = cos(hue);
    return vec3(color * cosAngle + cross(k, color) * sin(hue) + k * dot(k, color) * (1.0 - cosAngle));
}

vec3 SampleBaseColor(vec2 off, uvec4 indices, vec4 tiling, vec2 coords) {
    
    vec3 q00 = nonuniformEXT(texture(baseColorMaps, vec3(coords * tiling.x, nonuniformEXT(float(indices.x))), globalData.mipLodBias).rgb);
    vec3 q10 = nonuniformEXT(indices.y != indices.x ? texture(baseColorMaps, vec3(coords * tiling.y, nonuniformEXT(float(indices.y))), globalData.mipLodBias).rgb : q00);
    vec3 q01 = nonuniformEXT(indices.z != indices.x ? texture(baseColorMaps, vec3(coords * tiling.z, nonuniformEXT(float(indices.z))), globalData.mipLodBias).rgb : q00);
    vec3 q11 = nonuniformEXT(indices.w != indices.x ? texture(baseColorMaps, vec3(coords * tiling.w, nonuniformEXT(float(indices.w))), globalData.mipLodBias).rgb : q00);
    
    // Interpolate samples horizontally
    vec3 h0 = mix(q00, q10, off.x);
    vec3 h1 = mix(q01, q11, off.x);
    
    // Interpolate samples vertically
    return mix(h0, h1, off.y);    
    
}

float SampleRoughness(vec2 off, uvec4 indices, vec4 tiling, vec2 coords) {

    float q00 = nonuniformEXT(texture(roughnessMaps, vec3(coords * tiling.x, nonuniformEXT(float(indices.x))), globalData.mipLodBias).r);
    float q10 = nonuniformEXT(indices.y != indices.x ? texture(roughnessMaps, vec3(coords * tiling.y, nonuniformEXT(float(indices.y))), globalData.mipLodBias).r : q00);
    float q01 = nonuniformEXT(indices.z != indices.x ? texture(roughnessMaps, vec3(coords * tiling.z, nonuniformEXT(float(indices.z))), globalData.mipLodBias).r : q00);
    float q11 = nonuniformEXT(indices.w != indices.x ? texture(roughnessMaps, vec3(coords * tiling.w, nonuniformEXT(float(indices.w))), globalData.mipLodBias).r : q00);
    
    // Interpolate samples horizontally
    float h0 = mix(q00, q10, off.x);
    float h1 = mix(q01, q11, off.x);
    
    // Interpolate samples vertically
    return mix(h0, h1, off.y);    
    
}

float SampleAo(vec2 off, uvec4 indices, vec4 tiling, vec2 coords) {
    
    float q00 = nonuniformEXT(texture(aoMaps, vec3(coords * tiling.x, nonuniformEXT(float(indices.x))), globalData.mipLodBias).r);
    float q10 = nonuniformEXT(indices.y != indices.x ? texture(aoMaps, vec3(coords * tiling.y, nonuniformEXT(float(indices.y))), globalData.mipLodBias).r : q00);
    float q01 = nonuniformEXT(indices.z != indices.x ? texture(aoMaps, vec3(coords * tiling.z, nonuniformEXT(float(indices.z))), globalData.mipLodBias).r : q00);
    float q11 = nonuniformEXT(indices.w != indices.x ? texture(aoMaps, vec3(coords * tiling.w, nonuniformEXT(float(indices.w))), globalData.mipLodBias).r : q00);
    
    // Interpolate samples horizontally
    float h0 = mix(q00, q10, off.x);
    float h1 = mix(q01, q11, off.x);
    
    // Interpolate samples vertically
    return mix(h0, h1, off.y);    
    
}

vec3 SampleNormal(vec2 off, uvec4 indices, vec4 tiling, vec2 coords) {

    vec3 q00 = nonuniformEXT(texture(normalMaps, vec3(coords * tiling.x, nonuniformEXT(float(indices.x))), globalData.mipLodBias).rgb);
    vec3 q10 = nonuniformEXT(indices.y != indices.x ? texture(normalMaps, vec3(coords * tiling.y, nonuniformEXT(float(indices.y))), globalData.mipLodBias).rgb : q00);
    vec3 q01 = nonuniformEXT(indices.z != indices.x ? texture(normalMaps, vec3(coords * tiling.z, nonuniformEXT(float(indices.z))), globalData.mipLodBias).rgb : q00);
    vec3 q11 = nonuniformEXT(indices.w != indices.x ? texture(normalMaps, vec3(coords * tiling.w, nonuniformEXT(float(indices.w))), globalData.mipLodBias).rgb : q00);
    
    // Interpolate samples horizontally
    vec3 h0 = mix(q00, q10, off.x);
    vec3 h1 = mix(q01, q11, off.x);
    
    // Interpolate samples vertically
    return mix(h0, h1, off.y);    
    
}

float Interpolate(float q00, float q10, float q01, float q11, vec2 off) {

    // Interpolate samples horizontally
    float h0 = mix(q00, q10, off.x);
    float h1 = mix(q01, q11, off.x);
    
    // Interpolate samples vertically
    return mix(h0, h1, off.y);    

}

void main() {

    uvec4 indices;
    vec2 coords = materialTexCoords;

    vec2 tex = materialTexCoords / PushConstants.tileScale;
    vec2 off = tex - floor(tex);

    off = vec2(off.x + (0.5 * sin(coords.y)
        + 0.7 * cos(coords.y)) / PushConstants.tileScale,
        off.y + (0.4 * cos(coords.x * 2.0) + 0.6 * cos(coords.x)) / PushConstants.tileScale);
        
    vec2 splatOffset = floor(off);
    off = off - floor(off);

    splatOffset = vec2(0.0);
    off = tex - floor(tex);

    float texel = 1.0 / (8.0 * PushConstants.patchSize);
    tex = nonuniformEXT((floor(coords / PushConstants.nodeSideLength / texel) + splatOffset) * texel);
    indices.x = nonuniformEXT(textureLod(splatMap, tex, 0).r);
    indices.y = nonuniformEXT(textureLod(splatMap, tex + vec2(texel, 0.0), 0).r);
    indices.z = nonuniformEXT(textureLod(splatMap, tex + vec2(0.0, texel), 0).r);
    indices.w = nonuniformEXT(textureLod(splatMap, tex + vec2(texel, texel), 0).r);

    float noiseX = textureLod(noiseMap, 0.25 * materialTexCoords, 0).r;
    float noiseY = textureLod(noiseMap, 0.25* materialTexCoords + vec2(0.5), 0).r;
    float noiseZ = textureLod(noiseMap, 0.1 * materialTexCoords + vec2(0.75), 0).r;
    float noiseW = textureLod(noiseMap, 0.005 * materialTexCoords + vec2(0.275), 0).r;

    vec2 flooredCoords = floor(coords + vec2(noiseX, noiseY));
    float rand = random(flooredCoords);
    
    vec2 noise = 20.0 * vec2(noiseX, noiseY);
    vec2 rotTexCoords = rotateUV(materialTexCoords, rand);
    
    vec4 tiling = vec4(
        Materials.materials[nonuniformEXT(indices.x)].tiling,
        Materials.materials[nonuniformEXT(indices.y)].tiling,
        Materials.materials[nonuniformEXT(indices.z)].tiling,
        Materials.materials[nonuniformEXT(indices.w)].tiling
    );

    baseColorFS = SampleBaseColor(off, indices, tiling, rotTexCoords);
    baseColorFS = hueShift(baseColorFS, noiseZ - 0.5);
    baseColorFS = baseColorFS * (noiseW * 0.5 + 0.5);
    //baseColorFS = vec3(noiseZ);
    
    float roughness = Interpolate(
        Materials.materials[nonuniformEXT(indices.x)].roughness,
        Materials.materials[nonuniformEXT(indices.y)].roughness,
        Materials.materials[nonuniformEXT(indices.z)].roughness,
        Materials.materials[nonuniformEXT(indices.w)].roughness,
        off
    );
    float metalness = Interpolate(
        Materials.materials[nonuniformEXT(indices.x)].metalness,
        Materials.materials[nonuniformEXT(indices.y)].metalness,
        Materials.materials[nonuniformEXT(indices.z)].metalness,
        Materials.materials[nonuniformEXT(indices.w)].metalness,
        off
        );
    float ao = Interpolate(
        Materials.materials[nonuniformEXT(indices.x)].ao,
        Materials.materials[nonuniformEXT(indices.y)].ao,
        Materials.materials[nonuniformEXT(indices.z)].ao,
        Materials.materials[nonuniformEXT(indices.w)].ao,
        off
        );
    
    materialIdxFS = Materials.materials[nonuniformEXT(indices.x)].idx;

    // We should move this to the tesselation evaluation shader
    // so we only have to calculate these normals once. After that 
    // we can pass a TBN matrix to this shader
    tex = vec2(PushConstants.normalTexelSize) + texCoords *
        (1.0 - 3.0 * PushConstants.normalTexelSize)
        + 0.5 * PushConstants.normalTexelSize;
    vec3 norm = 2.0 * texture(normalMap, tex).rgb - 1.0;

    geometryNormalFS = EncodeNormal(normalize(mat3(globalData.vMatrix) * norm));
    
#ifdef MATERIAL_MAPPING
    // Normal mapping only for near tiles
    float normalScale = Interpolate(
        Materials.materials[nonuniformEXT(indices.x)].normalScale,
        Materials.materials[nonuniformEXT(indices.y)].normalScale,
        Materials.materials[nonuniformEXT(indices.z)].normalScale,
        Materials.materials[nonuniformEXT(indices.w)].normalScale,
        off
        );
    vec3 normal = SampleNormal(off, indices, tiling, rotTexCoords);
    vec3 tang = vec3(1.0, 0.0, 0.0);
    tang.y = -((norm.x*tang.x) / norm.y) - ((norm.z*tang.z) / norm.y);
    tang = normalize(tang);
    vec3 bitang = normalize(cross(tang, norm));
    mat3 tbn = mat3(tang, bitang, norm);
    normal = normalize(tbn * (2.0 * normal - 1.0));
    normal = mix(norm, normal, normalScale);
    ao *= SampleAo(off, indices, tiling, rotTexCoords);
    roughness *= SampleRoughness(off, indices, tiling, rotTexCoords);
#else
    vec3 normal = norm;
#endif
    
    normalFS = EncodeNormal(normalize(mat3(globalData.vMatrix) * normal));
    roughnessMetalnessAoFS = vec3(roughness, metalness, ao);
    
    // Calculate velocity
    vec2 ndcL = ndcLast.xy / ndcLast.z;
    vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;
    
}