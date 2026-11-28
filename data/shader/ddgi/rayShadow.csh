#define LOCAL_STACK

#include <../raytracer/tracing.hsh>
#include <ddgi.hsh>
#include <shadowRay.hsh>

layout (local_size_x = 32) in;

layout(std430, set = 3, binding = 2) buffer RayHits {
    PackedRayHit hits[];
};

layout(std430, set = 3, binding = 4) buffer ShadowRays {
    PackedShadowRay shadowRays[];
};

layout(std430, set = 3, binding = 5) buffer ShadowRayCount {
    uint shadowRayCount;
};

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= shadowRayCount)
        return;

    Ray ray;
    ray.origin = shadowRays[idx].origin.xyz;
    ray.direction = shadowRays[idx].direction.xyz;

    if (!HitAny(ray, INSTANCE_MASK_SHADOW, 0.0, shadowRays[idx].origin.w)) {
        uint hitIdx = floatBitsToUint(shadowRays[idx].radiance.w);
        RayHit hit = UnpackRayHit(hits[hitIdx]);
        hit.radiance.rgb = AeF16x3(vec3(hit.radiance.rgb) + shadowRays[idx].radiance.rgb);
        hits[hitIdx] = PackRayHit(hit);
    }

}
