#include <globals.hsh>
#include <common/convert.hsh>
#include <common/PI.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

#ifndef ENVIRONMENT_PROBE
layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D colorImage;
layout(set = 3, binding = 1, rg16f) writeonly uniform image2D velocityImage;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
#else
layout(set = 3, binding = 0, rgba16f) writeonly uniform imageCube colorImage;
#endif

#define iSteps 20
#define jSteps 10

layout(set = 3, binding = 3, std140) uniform UniformBuffer {
    mat4 ivMatrix;
    mat4 ipMatrix;
    vec4 cameraLocation;
    vec4 planetCenter;
    vec4 sunDirection;
    float sunIntensity;
    float planetRadius;
    float atmosphereRadius;
} uniforms;

#ifdef ENVIRONMENT_PROBE
layout(set = 3, binding = 4, std140) uniform MatricesBuffer {
    mat4 data[6];
} matrices;
#endif

vec2 resolution = vec2(imageSize(colorImage));

const float rayScaleHeight = 8.0e3;
const float mieScaleHeight = 1.2e3;

vec3 planetCenter = -vec3(0.0, uniforms.planetRadius, 0.0);

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos,
    vec3 kRlh, float kMie, out vec3 totalRlh, out vec3 totalMie);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(colorImage).x ||
        pixel.y > imageSize(colorImage).y)
        return;

#ifndef ENVIRONMENT_PROBE
    float depth = texelFetch(depthTexture, pixel, 0).r;
    if (depth < 1.0)
        return;
#else
    float depth = 1.0;
#endif
    vec2 texCoord = (vec2(pixel) + 0.5) / resolution;

    // Don't use the global inverse matrices here, since we also render the cubemap with this shader
	vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord, uniforms.ipMatrix);
#ifndef ENVIRONMENT_PROBE
    vec3 worldPos = vec3(uniforms.ivMatrix * vec4(viewPos, 1.0));
#else
    vec3 worldPos = vec3(matrices.data[gl_GlobalInvocationID.z] * vec4(viewPos, 1.0));
#endif

    const float g = 0.76;
    vec3 r = normalize(worldPos - uniforms.cameraLocation.xyz);
    vec3 pSun = normalize(-uniforms.sunDirection.xyz);

    vec3 totalRlh;
    vec3 totalMie;

    atmosphere(
        r,           // normalized ray direction
        uniforms.cameraLocation.xyz,               // ray origin
        -uniforms.sunDirection.xyz,                        // position of the sun
        uniforms.planetRadius,                         // radius of the planet in meters
        uniforms.atmosphereRadius,                         // radius of the atmosphere in meters
        vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
        21e-6,                          // Mie scattering coefficient
        totalRlh,
        totalMie
    );

    // Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    vec3 color = max(pRlh * totalRlh + pMie * totalMie, vec3(0.0));

    float LdotV = max(0.0, dot(normalize(pSun), normalize(r)));
    float sunAngle = cos(2.0 * 0.5 * 3.14 / 180.0);

    if (LdotV > sunAngle) {
        float sunDisk = clamp(0.5 * (LdotV - sunAngle) / (1.0 - sunAngle), 0.0, 1.0);
        color += uniforms.sunIntensity * sunDisk;
    }

#ifndef ENVIRONMENT_PROBE
    // Calculate velocity
    vec3 ndcCurrent = (globalData.pMatrix * vec4(viewPos, 1.0)).xyw;
    vec3 ndcLast = (globalData.pvMatrixLast * vec4(worldPos, 1.0)).xyw;

    vec2 ndcL = ndcLast.xy / ndcLast.z;
    vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    vec2 velocity = (ndcL - ndcC) * 0.5;

    imageStore(velocityImage, pixel, vec4(velocity, 0.0, 1.0));
    imageStore(colorImage, pixel, vec4(color, 1.0));
#else
    imageStore(colorImage, ivec3(pixel, int(gl_GlobalInvocationID.z)), vec4(color, 1.0));
#endif

}

vec2 IntersectSphere(vec3 origin, vec3 direction, vec3 pos, float radius) {

    vec3 L = pos - origin;
    float DT = dot(L, direction);
    float r2 = radius * radius;

    float ct2 = dot(L, L) - DT * DT;

    if (ct2 > r2)
    return vec2(-1.0);

    float AT = sqrt(r2 - ct2);
    float BT = AT;

    float AO = DT - AT;
    float BO = DT + BT;

    float minDist = min(AO, BO);
    float maxDist = max(AO, BO);

    return vec2(minDist, maxDist);
}

void CalculateRayLength(vec3 rayOrigin, vec3 rayDirection, out float minDist, out float maxDist) {

    vec2 planetDist = IntersectSphere(rayOrigin, rayDirection, planetCenter, uniforms.planetRadius);
    vec2 atmosDist = IntersectSphere(rayOrigin, rayDirection, planetCenter, uniforms.atmosphereRadius);

    // We're in the in the planet
    if (planetDist.x < 0.0 && planetDist.y >= 0.0) {
        // When the planet is in front of the inner layer set dist to zero
        minDist = 0.0;
        maxDist = 0.0;
    }
    else {
        // We're in the atmosphere layer
        if (atmosDist.x < 0.0 && atmosDist.y >= 0.0) {
            // When the planet is in front of the inner layer set dist to zero
            minDist = 0.0;
            maxDist = planetDist.x >= 0.0 ? min(planetDist.x, atmosDist.y) : atmosDist.y;
        }
        else {
            // Out of the atmosphere
            minDist = max(0.0, atmosDist.x);
            maxDist = planetDist.x >= 0.0 ? max(0.0, planetDist.x) : max(0.0, atmosDist.y);
        }
    }

}

bool LightSampling(vec3 origin,
vec3 sunDirection,
float planetRadius,
float atmosRadius,
out float opticalDepthMie,
out float opticalDepthRay) {

    float inDist, outDist;
    CalculateRayLength(origin, sunDirection, inDist, outDist);

    float time = 0.0;

    opticalDepthMie = 0.0;
    opticalDepthRay = 0.0;

    float stepSize = (outDist - inDist) / float(jSteps);

    for (int i = 0; i < jSteps; i++) {

        vec3 pos = origin + sunDirection * (time + 0.5 * stepSize);

        float height = distance(planetCenter, pos) - planetRadius;

        if (height < 0.0)
        return false;

        opticalDepthMie += exp(-height / mieScaleHeight) * stepSize;
        opticalDepthRay += exp(-height / rayScaleHeight) * stepSize;

        time += stepSize;

    }

    return true;

}

void atmosphere(vec3 r, vec3 r0, vec3 pSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, out vec3 totalRlh, out vec3 totalMie) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);

    totalRlh = vec3(0.0);
    totalMie = vec3(0.0);

    float inDist, outDist;
    CalculateRayLength(r0, r, inDist, outDist);
    if (inDist <= 0.0 && outDist <= 0.0)
    return;

    float iStepSize = (outDist - inDist) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = inDist;

    // Initialize accumulators for Rayleigh and Mie scattering.

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;

    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

        // Calculate the height of the sample.
        float iHeight = distance(iPos, planetCenter) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / rayScaleHeight) * iStepSize;
        float odStepMie = exp(-iHeight / mieScaleHeight) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        bool overground = LightSampling(iPos, pSun, rPlanet, rAtmos, jOdMie, jOdRlh);

        if (overground) {
            // Calculate attenuation.
            vec3 transmittance = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

            // Accumulate scattering.
            totalRlh += odStepRlh * transmittance;
            totalMie += odStepMie * transmittance;
        }

        // Increment the primary ray time.
        iTime += iStepSize;

    }

    totalMie = totalMie * uniforms.sunIntensity * kMie;
    totalRlh = totalRlh * uniforms.sunIntensity * kRlh;

}