#include <brdf.hsh>
#include <importanceSample.hsh>
#include <preintegrate.hsh>
#include <../common/utility.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 0, binding = 0, rgba16f) writeonly uniform image2D textureOut;

const uint sampleCount = 4096u;

vec3 DFG(float NdotV, float alpha) {

    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    vec3 acc = vec3(0.0);

    for (uint i = 0u; i < sampleCount; i++) {

        vec2 Xi = Hammersley(i, sampleCount);

        // Specular pre-integration
        vec3 H = ImportanceSampleGGX(Xi, N, alpha);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float VdotH = max(0.0, dot(V, H));
        float NdotL = max(0.0, L.z);
        float NdotH = max(0.0, H.z);

        if (NdotL > 0.0) {
            float G = GDFG(NdotV, NdotL, alpha);
            float Gv = G * VdotH / NdotH;
            float Fc = pow(1.0 - VdotH, 5.0);
            acc.x += Gv * (1.0 - Fc);
            acc.y += Gv * Fc;
        }

        // Diffuse pre-intergration
        Xi = fract(Xi + 0.5);
        float pdf;

        ImportanceSampleCosDir(N, Xi, L, NdotL, pdf);
        if (NdotL > 0.0) {
            float LdotH = saturate(dot(L, normalize(V + L)));
            float NdotV = saturate(dot(N, V));
            acc.z += RenormalizedDisneyDiffuse(NdotV, NdotL, LdotH, sqrt(alpha));
        }
    
    }

    return acc / float(sampleCount);

}

void main() {

    ivec2 size = imageSize(textureOut);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x &&
        coord.y < size.y) {

        float NdotV = (float(coord.x) + 0.5) / float(size.x);
        float roughness = (float(coord.y) + 0.5) / float(size.y);
        
        // We can later sample with the linear/perceptual roughness
        // If we want to sample with roughness, just remove the square
        vec3 dfg = DFG(NdotV, roughness * roughness);

        imageStore(textureOut, coord, vec4(dfg, 1.0));
        
    }

}