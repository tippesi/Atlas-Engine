#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (set = 3, binding = 0, rg32f) readonly uniform image2D twiddleIndicesTexture;

layout (set = 3, binding = 1, rgba32f) readonly uniform image2DArray pingpong0;
layout (set = 3, binding = 2, rgba32f) writeonly uniform image2DArray pingpong1;

layout(push_constant) uniform constants {
    int stage;
    int pingpong;
    int N;
    float preTwiddle;
} PushConstants;

void horizontal();
void vertical();

void main() {

#ifdef HORIZONTAL
    horizontal();
#endif
#ifdef VERTICAL
    vertical();
#endif

}

void horizontal() {

    int spectrumIdx = int(gl_GlobalInvocationID.z);
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    
    float k = mod(float(coord.x) * PushConstants.preTwiddle, float(PushConstants.N));
    float twiddleArgument = 2.0 * PI * k / float(PushConstants.N);
    vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));
    
    vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(PushConstants.stage, coord.x)).rg;
        
    vec4 s1 = imageLoad(pingpong0, ivec3(twiddle.x, coord.y, spectrumIdx));
    vec4 t1 = imageLoad(pingpong0, ivec3(twiddle.y, coord.y, spectrumIdx));
        
    vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
        twiddleFactor.x, twiddleFactor.y);
        
    vec4 H1 = s1 + mul(u1, t1);
        
    imageStore(pingpong1, ivec3(gl_GlobalInvocationID.xyz), H1);

}

void vertical() {

    int spectrumIdx = int(gl_GlobalInvocationID.z);
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    
    float k = mod(float(coord.y) * PushConstants.preTwiddle, float(PushConstants.N));
    float twiddleArgument = 2.0 * PI * k / float(PushConstants.N);
    vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));
    
    vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(PushConstants.stage, coord.y)).rg;
        
    vec4 s1 = imageLoad(pingpong0, ivec3(coord.x, twiddle.x, spectrumIdx));
    vec4 t1 = imageLoad(pingpong0, ivec3(coord.x, twiddle.y, spectrumIdx));
        
    vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
        twiddleFactor.x, twiddleFactor.y);
        
    vec4 H1 = s1 + mul(u1, t1);
        
    imageStore(pingpong1, ivec3(gl_GlobalInvocationID.xyz), H1);


}