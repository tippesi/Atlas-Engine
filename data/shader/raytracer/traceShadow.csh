#define LOCAL_STACK

#include <tracing.hsh>
#include <deferredShadow.hsh>

layout (local_size_x = 32) in;

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= deferredShadowRayCount)
        return;

    Ray ray;
    ray.origin = deferredShadowRays[idx].origin.xyz;
    ray.direction = deferredShadowRays[idx].direction.xyz;

    uint lightIdx = floatBitsToUint(deferredShadowRays[idx].direction.w);
    deferredLights[lightIdx].w = HitAnyTransparency(ray, INSTANCE_MASK_SHADOW, 0.0, deferredShadowRays[idx].origin.w);

}
