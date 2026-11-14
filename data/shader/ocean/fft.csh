#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = FFT_SIZE / 2, local_size_y = 1, local_size_z = 1) in;

#ifdef HORIZONTAL
layout (set = 3, binding = 0, rgba32f) writeonly uniform image2DArray hTD;
layout (set = 3, binding = 1, rg32f) readonly uniform image2DArray h0K;
#ifdef NORMALS
layout (set = 3, binding = 2, rgba32f) writeonly uniform image2DArray hTDSlopes;
#endif
#endif
#ifdef VERTICAL
layout (set = 3, binding = 0, rgba16f) writeonly uniform image2DArray displacementMap;
layout (set = 3, binding = 1, rgba32f) readonly uniform image2DArray hTD;
#ifdef NORMALS
layout (set = 3, binding = 2, rgba16f) writeonly uniform image2DArray normalMap;
layout (set = 3, binding = 3, rgba32f) readonly uniform image2DArray hTDSlopes;
#endif
#endif

layout(push_constant) uniform constants {
    vec4 L;
    vec4 spectrumScaling;
    float time;
    float choppyScale;
    float displacementScale;
} PushConstants;

const float g = 9.81;

shared vec4 sharedData[FFT_SIZE];
#ifdef NORMALS
shared vec4 sharedSlopes[FFT_SIZE];
#endif

int ReverseBits(int idx) {

    return int(bitfieldReverse(uint(idx)) >> uint(32 - findMSB(FFT_SIZE)));

}

#ifdef HORIZONTAL
vec2 MulI(vec2 c) {

    return vec2(-c.y, c.x);

}

vec4 ComputeSpectrum(ivec3 coord, out vec4 slopes) {

    vec2 waveCoord = vec2(coord.xy) - float(FFT_SIZE) / 2.0;
    vec2 waveVector = 2.0 * PI / PushConstants.L[coord.z] * waveCoord;

    float k = length(waveVector);
    float w = sqrt(g * k);

    ivec2 inverseCoords = (FFT_SIZE - coord.xy) % FFT_SIZE;
    vec2 h0_k = imageLoad(h0K, coord).rg;
    vec2 h0_mk = imageLoad(h0K, ivec3(inverseCoords, coord.z)).rg;

    float cos_wt = cos(w * PushConstants.time);
    float sin_wt = sin(w * PushConstants.time);

    // Calculate h(t)
    vec2 ht;
    ht.x = cos_wt * (h0_k.x + h0_mk.x) - sin_wt * (h0_k.y + h0_mk.y);
    ht.y = sin_wt * (h0_k.x - h0_mk.x) + cos_wt * (h0_k.y - h0_mk.y);

    // From h(t) calculate dx(t) and dy(t) as i * k / |k| * h(t), which moves the points towards the crests
    float kt2 = dot(waveCoord, waveCoord);
    vec2 nkt = vec2(0.0);

    if (kt2 > 1e-12)
        nkt = normalize(waveCoord);

    vec2 dt_x = vec2(-ht.y * nkt.x, ht.x * nkt.x);
    vec2 idt_z = vec2(-ht.x * nkt.y, -ht.y * nkt.y);

#ifdef NORMALS
    // Real results are packed in pairs: h + i * dDx/dz, dh/dx + i * dh/dz and dDx/dx + i * dDz/dz
    vec2 slopeX = MulI(ht) * waveVector.x;
    vec2 slopeZ = MulI(ht) * waveVector.y;
    vec2 dxx = -ht * waveVector.x * nkt.x;
    vec2 dzz = -ht * waveVector.y * nkt.y;
    vec2 dxz = -ht * waveVector.x * nkt.y;

    slopes = vec4(slopeX + MulI(slopeZ), dxx + MulI(dzz));
    ht += MulI(dxz);
#else
    slopes = vec4(0.0);
#endif

    return vec4(ht, dt_x + idt_z);

}
#endif

void FFT() {

    int idx = int(gl_LocalInvocationIndex);

    for (int span = 1; span < FFT_SIZE; span *= 2) {
        int pos = idx % span;
        int top = (idx / span) * span * 2 + pos;
        int bottom = top + span;

        float twiddleArgument = PI * float(pos) / float(span);
        vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));

        vec4 s = sharedData[top];
        vec4 t = mul(vec4(twiddleFactor, twiddleFactor), sharedData[bottom]);

        sharedData[top] = s + t;
        sharedData[bottom] = s - t;

#ifdef NORMALS
        s = sharedSlopes[top];
        t = mul(vec4(twiddleFactor, twiddleFactor), sharedSlopes[bottom]);

        sharedSlopes[top] = s + t;
        sharedSlopes[bottom] = s - t;
#endif

        barrier();
    }

}

void main() {

    int idx = int(gl_LocalInvocationIndex);
    int spectrumIdx = int(gl_WorkGroupID.z);

#ifdef HORIZONTAL
    int row = int(gl_WorkGroupID.y);

    for (int i = idx; i < FFT_SIZE; i += FFT_SIZE / 2) {
        vec4 slopes;
        sharedData[ReverseBits(i)] = ComputeSpectrum(ivec3(i, row, spectrumIdx), slopes);
#ifdef NORMALS
        sharedSlopes[ReverseBits(i)] = slopes;
#endif
    }

    barrier();

    FFT();

    for (int i = idx; i < FFT_SIZE; i += FFT_SIZE / 2) {
        imageStore(hTD, ivec3(i, row, spectrumIdx), sharedData[i]);
#ifdef NORMALS
        imageStore(hTDSlopes, ivec3(i, row, spectrumIdx), sharedSlopes[i]);
#endif
    }
#endif

#ifdef VERTICAL
    int column = int(gl_WorkGroupID.x);

    for (int i = idx; i < FFT_SIZE; i += FFT_SIZE / 2) {
        sharedData[ReverseBits(i)] = imageLoad(hTD, ivec3(column, i, spectrumIdx));
#ifdef NORMALS
        sharedSlopes[ReverseBits(i)] = imageLoad(hTDSlopes, ivec3(column, i, spectrumIdx));
#endif
    }

    barrier();

    FFT();

    for (int i = idx; i < FFT_SIZE; i += FFT_SIZE / 2) {
        vec4 data = sharedData[i];
        float perm = bool((column + i) & 1) ? -1.0 : 1.0;
        imageStore(displacementMap, ivec3(column, i, spectrumIdx), vec4(perm * data.bra, 1.0));

#ifdef NORMALS
        vec4 slopes = perm * sharedSlopes[i];
        float scale = PushConstants.spectrumScaling[spectrumIdx];
        float choppyScale = PushConstants.choppyScale * scale;

        // Same as the normal shader, but without the error of finite differences
        vec2 slope = slopes.xy * PushConstants.displacementScale * scale;
        vec3 derivatives = vec3(slopes.zw, perm * data.g) * choppyScale;
        float J = (1.0 + derivatives.x) * (1.0 + derivatives.y) - derivatives.z * derivatives.z;

        vec2 gradient = slope / max(vec2(1.0) + derivatives.xy, vec2(0.01));
        imageStore(normalMap, ivec3(column, i, spectrumIdx), vec4(-gradient, dot(gradient, gradient), 1.0 - J));
#endif
    }
#endif

}
