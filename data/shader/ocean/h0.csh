#include <../common/PI.hsh>

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (set = 3, binding = 0, rg32f) writeonly uniform image2DArray h0K;

layout(set = 3, binding = 2) uniform sampler2D noise0;
layout(set = 3, binding = 3) uniform sampler2D noise1;
layout(set = 3, binding = 4) uniform sampler2D noise2;
layout(set = 3, binding = 5) uniform sampler2D noise3;

layout(push_constant) uniform constants {
    ivec4 L;
    vec2 w;
    int N;
    float A;
    float windspeed;
    float windDependency;
    float waveSurpression;
} PushConstants;

const float g = 9.81;

// Box-Muller transform: https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
vec2 gaussrnd() {

    vec2 coord = vec2(gl_GlobalInvocationID.xy) / float(PushConstants.N);
    
    float U00 = clamp(texture(noise0, coord).r + 0.00001, 0.0, 1.0);
    float U01 = clamp(texture(noise1, coord).r + 0.00001, 0.0, 1.0);
    
    float R0 = sqrt(-2.0 * log(U00));
    float T0 = 2.0 * PI * U01;
    
    return vec2(R0 * cos(T0), R0 * sin(T0));

}

float phillips(vec2 k, float A, float l, vec2 w) {

    float k2 = dot(k, k);
    float kCos = dot(k, normalize(PushConstants.w));
    float phillips = PushConstants.A / (k2 * k2 * k2) * (kCos * kCos) * exp(-1.0 / (k2 * l * l));
    
    if (kCos < 0.0)
        phillips *= PushConstants.windDependency;
    
    // Surpress small waves
    l *= PushConstants.waveSurpression;

    return phillips * exp(-k2 * l * l);

}

void main() {
    
    int spectrumIdx = int(gl_GlobalInvocationID.z);

    vec2 coord = vec2(gl_GlobalInvocationID.xy) - float(PushConstants.N) / 2.0;
    
    vec2 waveVector = 2.0 * PI / float(PushConstants.L[spectrumIdx]) * coord;
    
    float l = (PushConstants.windspeed * PushConstants.windspeed) / g;
    
    float h0k = sqrt(phillips(waveVector, PushConstants.A, l, PushConstants.w)) / sqrt(2.0);
    
    vec2 rnd = gaussrnd();
    
    if (waveVector.x == 0.0 &&
        waveVector.y == 0.0) {
        h0k = 0.0;
    }
    
    imageStore(h0K, ivec3(gl_GlobalInvocationID.xyz), vec4(rnd.xy * h0k, 0.0, 0.0));
    
}