#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (set = 3, binding = 0, rgba32f) writeonly uniform image2DArray hTD;
layout (set = 3, binding = 1, rg32f) readonly uniform image2DArray h0K;

layout(push_constant) uniform constants {
    ivec4 L;
    int N;
    float time;
} PushConstants;

const float g = 9.81;

void main() {

    int spectrumIdx = int(gl_GlobalInvocationID.z);
    vec2 coord = vec2(gl_GlobalInvocationID.xy) - float(PushConstants.N) / 2.0;
    
    vec2 waveVector = 2.0 * PI / float(PushConstants.L[spectrumIdx]) * coord;
    
    float k = length(waveVector);    
    float w = sqrt(g * k);
    
    ivec2 inverseCoords = (ivec2(imageSize(h0K))) - ivec2(gl_GlobalInvocationID);    
    vec2 h0_k = imageLoad(h0K, ivec3(gl_GlobalInvocationID.xy, spectrumIdx)).rg;
    vec2 h0_mk = imageLoad(h0K, ivec3(inverseCoords, spectrumIdx)).rg;
        
    float cos_wt = cos(w * PushConstants.time);
    float sin_wt = sin(w * PushConstants.time);

    // Calculate h(t)
    vec2 ht;
    ht.x = cos_wt * (h0_k.x + h0_mk.x) - sin_wt * (h0_k.y + h0_mk.y);
    ht.y = sin_wt * (h0_k.x - h0_mk.x) + cos_wt * (h0_k.y - h0_mk.y);

    // From h(t) calculate dx(t) and dy(t)
    vec2 kt = coord;
    float kt2 = dot(kt, kt);
    vec2 nkt = vec2(0.0);

    if (kt2 > 1e-12)
        nkt = normalize(kt);

    vec2 dt_x = vec2(ht.y * nkt.x, -ht.x * nkt.x);
    vec2 idt_z = vec2(ht.x * nkt.y, ht.y * nkt.y);
    
    imageStore(hTD, ivec3(gl_GlobalInvocationID.xyz), vec4(ht, dt_x + idt_z));
    
}