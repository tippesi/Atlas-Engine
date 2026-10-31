// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).
// Inspired partially by https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ddgi_updateCS.hlsl

#include <ddgi.hsh>

#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>

#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/barrier.hsh>
#include <../common/types.hsh>

#if defined(IRRADIANCE)
layout (local_size_x = 6, local_size_y = 6) in;
#elif defined(RADIANCE)
#ifdef LOWER_RES_RADIANCE
layout (local_size_x = 14, local_size_y = 14) in;
#else
layout (local_size_x = 30, local_size_y = 10) in;
#endif
#else
#ifdef LOWER_RES_MOMENTS
layout (local_size_x = 6, local_size_y = 6) in;
#else
layout (local_size_x = 14, local_size_y = 14) in;
#endif
#endif

#if defined(IRRADIANCE) || defined(RADIANCE)
layout (set = 3, binding = 0, rgb10_a2) writeonly uniform image2DArray probeImage;
#else
layout (set = 3, binding = 0, rg16f) writeonly uniform image2DArray probeImage;
#endif

layout(std430, set = 3, binding = 1) buffer RayHits {
    PackedRayHit hits[];
};

layout(std430, set = 3, binding = 2) buffer HistoryProbeStates {
    vec4 historyProbeStates[];
};

layout(std430, set = 3, binding = 3) buffer HistoryProbeOffsets {
    vec4 historyProbeOffsets[];
};

const uint sharedSize = 128;

#if defined(RADIANCE) && !defined(LOWER_RES_RADIANCE)
// Large probes are split into slices of rows, one work group each, such that several groups fit on a compute unit
const int probeSliceCount = 3;
#else
const int probeSliceCount = 1;
#endif

#if defined(IRRADIANCE) || defined(RADIANCE)
struct RayData {
    // This seems to be a better layout than two vec3's
    // My guess is that vec3's get expanded to vec4's
#ifndef AE_HALF_FLOAT
    vec4 direction;
    vec4 radiance;
#else
    AeF16x4 direction;
    AeF16x4 radiance;
#endif
};
#else
struct RayData {
    AeF16x4 direction;
};
#endif

shared RayData rayData[sharedSize];

#ifdef RADIANCE
// Each ray is linked into the lists of the texels within its weight cone, such that a texel only iterates its own rays
#ifdef LOWER_RES_RADIANCE
const int rayTexelReach = 1;
#else
const int rayTexelReach = 2;
#endif
const int rayTexelWidth = 2 * rayTexelReach + 1;
const uint texelsPerRay = uint(rayTexelWidth * rayTexelWidth);
const uint invalidRayNode = 0xFFFFFFFFu;
// Only rays within ~7 degrees pass the weight threshold
const float radianceCosThreshold = 0.992;

shared AeF16x4 texelDirections[gl_WorkGroupSize.x * gl_WorkGroupSize.y];
shared uint texelRayHeads[gl_WorkGroupSize.x * gl_WorkGroupSize.y];
shared ivec2 rayTexels[sharedSize];
shared uint rayNodeNext[sharedSize * texelsPerRay];

// Maps a texel outside of the octahedral map to the adjacent texel across the fold
ivec2 WrapOctahedronTexel(ivec2 texel, ivec2 res) {

    if (texel.x < 0 || texel.x >= res.x) {
        texel.x = texel.x < 0 ? -texel.x - 1 : 2 * res.x - 1 - texel.x;
        texel.y = res.y - 1 - texel.y;
    }
    if (texel.y < 0 || texel.y >= res.y) {
        texel.y = texel.y < 0 ? -texel.y - 1 : 2 * res.y - 1 - texel.y;
        texel.x = res.x - 1 - texel.x;
    }

    return texel;

}

// Each node is a pair of a ray and a texel in the neighborhood of the ray's texel
void LinkRayNode(uint node, ivec2 res, int sliceOffset) {

    uint rayIdx = node / texelsPerRay;
    uint neighbor = node % texelsPerRay;

    ivec2 rayTexel = rayTexels[rayIdx];
    ivec2 texel = rayTexel + ivec2(neighbor % uint(rayTexelWidth), neighbor / uint(rayTexelWidth)) - ivec2(rayTexelReach);
    ivec2 wrappedTexel = WrapOctahedronTexel(texel, res);
    // Near the middle of an edge the fold maps back into the neighborhood, link these texels once
    if (wrappedTexel != texel && all(lessThanEqual(abs(wrappedTexel - rayTexel), ivec2(rayTexelReach))))
        return;

    ivec2 sliceTexel = wrappedTexel - ivec2(0, sliceOffset);
    if (sliceTexel.y < 0 || sliceTexel.y >= int(gl_WorkGroupSize.y))
        return;

    uint texelIdx = uint(sliceTexel.y * res.x + sliceTexel.x);
    if (dot(texelDirections[texelIdx].rgb, rayData[rayIdx].direction.rgb) < AeF16(radianceCosThreshold))
        return;

    rayNodeNext[node] = atomicExchange(texelRayHeads[texelIdx], node);

}

void AccumulateRadiance(uint rayIdx, AeF16x3 N, bool reset, inout AeF16x4 result) {

    AeF16 cosTheta = dot(N, rayData[rayIdx].direction.rgb);
    if (!reset && cosTheta < AeF16(radianceCosThreshold))
        return;

    AeF16 weight = pow(max(AeF16(0.0), cosTheta), AeF16(256.0));

    AeF16x3 radiance = rayData[rayIdx].radiance.xyz;

    if (reset || weight > 0.15)
        result += AeF16x4(radiance, AeF16(1.0)) * weight;

}
#endif

vec4 GetHistoryFromHigherCascade(ivec2 pixel, ivec3 probeGroup, int cascadeIndex) {

    probeGroup.y %= ddgiData.volumeProbeCount.y;

    vec3 probePosition = vec3(probeGroup) * ddgiData.cascades[cascadeIndex].cellSize.xyz + ddgiData.cascades[cascadeIndex].volumeMin.xyz;

    // Note: Cascades are orderd from biggest (index 0) to smallest (index cascadeCount - 1)
    cascadeIndex = max(cascadeIndex - 1, 0);

    vec3 localPosition = probePosition - ddgiData.cascades[cascadeIndex].volumeMin.xyz;
    ivec3 cascadeProbeOffset = ivec3(0, cascadeIndex * ddgiData.volumeProbeCount.y, 0);

    ivec3 baseCell = ivec3(localPosition / ddgiData.cascades[cascadeIndex].cellSize.xyz) + cascadeProbeOffset;

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(baseCell, cascadeIndex, historyProbeCoord, reset);

#if defined(IRRADIANCE)
    ivec2 res = ivec2(ddgiData.volumeIrradianceRes);
#elif defined(RADIANCE)
    ivec2 res = ivec2(ddgiData.volumeRadianceRes);
#else
    ivec2 res = ivec2(ddgiData.volumeMomentsRes);
#endif

    ivec2 historyResOffset = (res + ivec2(2)) * ivec2(historyProbeCoord.xz) + ivec2(1);
    ivec3 historyVolumeCoord = ivec3(historyResOffset + pixel, int(historyProbeCoord.y));

#if defined(IRRADIANCE)
    vec3 lastResult = texelFetch(irradianceVolume, historyVolumeCoord, 0).rgb;
    return vec4(lastResult, 0.0); 
#elif defined(RADIANCE)
    vec3 lastResult = texelFetch(radianceVolume, historyVolumeCoord, 0).rgb;
    return vec4(lastResult, 0.0); 
#else
    vec2 lastResult = texelFetch(momentsVolume, historyVolumeCoord, 0).rg;
    return vec4(lastResult, 0.0, 0.0);
#endif

}

// Border texels repeat the mirrored interior texels of the same edge, such that bilinear filtering wraps around the octahedron
void StoreProbeTexel(ivec2 pix, ivec2 res, ivec2 resOffset, int layer, vec4 value) {

    ivec2 tileOffset = resOffset - ivec2(1);
    ivec2 texel = pix + ivec2(1);
    ivec2 mirrored = res + ivec2(1) - texel;

    imageStore(probeImage, ivec3(tileOffset + texel, layer), value);

    if (texel.y == 1)
        imageStore(probeImage, ivec3(tileOffset + ivec2(mirrored.x, 0), layer), value);
    if (texel.y == res.y)
        imageStore(probeImage, ivec3(tileOffset + ivec2(mirrored.x, res.y + 1), layer), value);
    if (texel.x == 1)
        imageStore(probeImage, ivec3(tileOffset + ivec2(0, mirrored.y), layer), value);
    if (texel.x == res.x)
        imageStore(probeImage, ivec3(tileOffset + ivec2(res.x + 1, mirrored.y), layer), value);

    bool cornerX = texel.x == 1 || texel.x == res.x;
    bool cornerY = texel.y == 1 || texel.y == res.y;
    if (cornerX && cornerY) {
        ivec2 corner = mix(ivec2(0), res + ivec2(1), equal(texel, ivec2(1)));
        imageStore(probeImage, ivec3(tileOffset + corner, layer), value);
    }

}

void main() {

    ivec3 probeGroup = ivec3(int(gl_WorkGroupID.x) / probeSliceCount, gl_WorkGroupID.yz);
    ivec3 probeGroupCount = ivec3(int(gl_NumWorkGroups.x) / probeSliceCount, gl_NumWorkGroups.yz);
    int sliceOffset = (int(gl_WorkGroupID.x) % probeSliceCount) * int(gl_WorkGroupSize.y);

    uint baseIdx = Flatten3D(probeGroup.xzy, probeGroupCount.xzy);
    int cascadeIndex = GetProbeCascadeIndex(baseIdx);

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(probeGroup, cascadeIndex, historyProbeCoord, reset);

    uint historyBaseIdx = Flatten3D(ivec3(historyProbeCoord.xzy), probeGroupCount.xzy);

    uint probeState = floatBitsToUint(historyProbeStates[historyBaseIdx].x);
    float probeAge = probeStates[baseIdx].w;
    vec4 probeOffset = reset ? vec4(0.0, 0.0, 0.0, 1.0) : historyProbeOffsets[historyBaseIdx];

    uint rayBaseIdx = baseIdx * ddgiData.rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint groupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

#if defined(IRRADIANCE)
    ivec2 res = ivec2(ddgiData.volumeIrradianceRes);
#elif defined(RADIANCE)
    ivec2 res = ivec2(ddgiData.volumeRadianceRes);
#else
    ivec2 res = ivec2(ddgiData.volumeMomentsRes);
#endif

    ivec2 pix = ivec2(gl_LocalInvocationID) + ivec2(0, sliceOffset);
    vec2 coord = (vec2(pix) + vec2(0.5)) / vec2(res);

    AeF16x3 N = AeF16x3(OctahedronToUnitVector(coord));
#ifdef RADIANCE
    texelDirections[gl_LocalInvocationIndex] = AeF16x4(N, 0.0);
#endif

    ivec2 resOffset = (res + ivec2(2)) * probeGroup.xz + ivec2(1);
    ivec3 volumeCoord = ivec3(resOffset + pix, probeGroup.y);

    AeF16 cellLength = AeF16(length(ddgiData.cascades[cascadeIndex].cellSize.xyz));
    AeF16 maxDepth = cellLength * AeF16(0.75);

    AeF16 depthSharpness = AeF16(ddgiData.depthSharpness);

    AeF16x4 result = AeF16x4(0.0);
    vec3 newProbeOffset = probeOffset.xyz;
    for (uint i = 0; i < probeRayCount; i += sharedSize) {
        // We might not have a multiple of shared size in terms of rays
        uint loadRayCount = min(sharedSize, probeRayCount - i);

#ifdef RADIANCE
        texelRayHeads[gl_LocalInvocationIndex] = invalidRayNode;
#endif

        // Load rays cooperatively
        for (uint j = gl_LocalInvocationIndex; j < loadRayCount; j += groupSize) {
            RayHit hit = UnpackRayHit(hits[rayBaseIdx + i + j]);
            rayData[j].direction.rgb = hit.direction.xyz;
#if defined(IRRADIANCE) || defined(RADIANCE)
            rayData[j].radiance.rgb = hit.radiance.rgb;
            rayData[j].direction.a = hit.radiance.a;
#ifdef RADIANCE
            rayTexels[j] = min(ivec2(UnitVectorToOctahedron(vec3(hit.direction.xyz)) * vec2(res)), res - ivec2(1));
#endif
#else
#ifndef AE_HALF_FLOAT
            float dist = hit.radiance.a;
            dist = dist < 0.0 ? dist * 0.2 : dist;

            float hitDistance = min(maxDepth, dist);
#else
            AeF16 dist = hit.radiance.a;
            dist = dist < AeF16(0.0) ? dist * AeF16(0.2) : dist;

            AeF16 hitDistance = min(maxDepth, dist);
#endif
            rayData[j].direction.w = hitDistance;
#endif
        }

        barrier();

#ifdef RADIANCE
        if (!reset) {
            for (uint node = gl_LocalInvocationIndex; node < loadRayCount * texelsPerRay; node += groupSize)
                LinkRayNode(node, res, sliceOffset);
            barrier();
        }
#endif

        float maxWeight = 0.0;

        // Iterate over all rays in the shared memory
#ifdef RADIANCE
        if (!reset) {
            for (uint node = texelRayHeads[gl_LocalInvocationIndex]; node != invalidRayNode; node = rayNodeNext[node])
                AccumulateRadiance(node / texelsPerRay, N, reset, result);
        }
        else {
            for (uint j = 0; j < loadRayCount; j++)
                AccumulateRadiance(j, N, reset, result);
        }
#else
        for (uint j = 0; j < loadRayCount; j++) {
            
#if defined(IRRADIANCE)
            AeF16 weight = max(AeF16(0.0), dot(N, rayData[j].direction.rgb));

            if (weight >= AeF16(0.00001)) {
                AeF16x3 radiance = AeF16x3(rayData[j].radiance);
                result += AeF16x4(radiance, AeF16(1.0)) * weight;    
            }

            const float probeOffsetDistance = max(ddgiData.cascades[cascadeIndex].cellSize.w * 0.05, 0.5);
            // Remember: Negative distances means backface hits.
            // Meaning we want to get probes from backfaces to the 
            // front and want to get a certain distance to these front
            // faces to gather more useful information per probe.
            // Each probe is dampend by its own factor, which is
            // reduced in each frame to stop them from moving indefinitely
            float dist = rayData[j].direction.a;
            if (probeOffset.w > 0.0) {
                float sig = sign(dist);
                if (dist < 0.0 && -dist < probeOffsetDistance && ddgiData.optimizeProbes > 0) {
                    //newProbeOffset -= rayData[j].direction.xyz * (sig * probeOffsetDistance - dist) * 0.02 * probeOffset.w / probeOffsetDistance;
                }

                // This might be used to move probes closer to geometry and make the visibility test more effective, disabled for now
                if (dist > 0.0 && dist < probeOffsetDistance && ddgiData.optimizeProbes > 0) {
                    newProbeOffset -= rayData[j].direction.xyz * (probeOffsetDistance - dist) * 0.001 * probeOffset.w / probeOffsetDistance;
                }
            }
#else
            AeF16 weight = max(AeF16(0.0), dot(N, rayData[j].direction.xyz));

            AeF16 hitDistance = rayData[j].direction.w;

            weight = pow(weight, depthSharpness);
            if (weight >= AeF16(0.00000001)) {
                result += AeF16x4(hitDistance, sqr(hitDistance), AeF16(0.0), AeF16(1.0)) * weight;
            }
#endif
        }
#endif

        barrier();

    }

    ivec2 historyResOffset = (res + ivec2(2)) * ivec2(historyProbeCoord.xz) + ivec2(1);
    ivec3 historyVolumeCoord = ivec3(historyResOffset + pix, int(historyProbeCoord.y));

    // Use a dynamic hysteris based on probe age to accumulate more efficiently at the beginning of a probes life
    float hysteresis = clamp(probeAge / (probeAge + 1.0), 0.0, ddgiData.hysteresis);

#if defined(IRRADIANCE)
    vec3 lastResult = texelFetch(irradianceVolume, historyVolumeCoord, 0).rgb;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, probeGroup, cascadeIndex).rgb;
    }
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        fullResult.xyz /= fullResult.w;
        fullResult.xyz = pow(fullResult.xyz, vec3(1.0 / ddgiData.volumeGamma));

        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
        else {                
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
    }
    StoreProbeTexel(pix, res, resOffset, volumeCoord.z, vec4(resultOut, 0.0));

    if (gl_LocalInvocationIndex == 0) {
        vec3 maxOffset = ddgiData.cascades[cascadeIndex].cellSize.xyz * 0.5;
        probeOffset.xyz = clamp(newProbeOffset, -maxOffset, maxOffset);
        probeOffset.w = 1.0;
        probeOffsets[baseIdx] = probeOffset;
    }
#elif defined(RADIANCE)
    vec3 lastResult = texelFetch(radianceVolume, historyVolumeCoord, 0).rgb;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, probeGroup, cascadeIndex).rgb;
    }
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        fullResult.xyz /= fullResult.w;
        fullResult.xyz = pow(fullResult.xyz, vec3(1.0 / ddgiData.volumeGamma));

        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
        else {                
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
    }
    StoreProbeTexel(pix, res, resOffset, volumeCoord.z, vec4(resultOut, 0.0));
#else
    vec2 lastResult = texelFetch(momentsVolume, historyVolumeCoord, 0).rg;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, probeGroup, cascadeIndex).rg;
    }
    vec2 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        fullResult.xy /= fullResult.w;
        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = mix(fullResult.xy, lastResult, hysteresis);
        }
        else {
            resultOut = mix(fullResult.xy, lastResult, hysteresis);
        }
    }

    StoreProbeTexel(pix, res, resOffset, volumeCoord.z, vec4(resultOut, 0.0, 0.0));
#endif

}