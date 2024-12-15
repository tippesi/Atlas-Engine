// Based on https://bruop.github.io/exposure/

#include <exposure.hsh>

layout (local_size_x = 256) in;

layout(set = 3, binding = 0, r32f) uniform image2D exposureImage;

layout(push_constant) uniform constants {
    float logLuminanceMin;
    float logLuminanceRange;
    float timeCoefficient;
    float pixelCount;
} pushConstants;

shared uint sharedHistogram[histogramBinCount];

void main() {

    uint countForThisBin = histogram[gl_LocalInvocationIndex];
    sharedHistogram[gl_LocalInvocationIndex] = countForThisBin * gl_LocalInvocationIndex;

    barrier();

    // Needs to be reset for the next frame
    histogram[gl_LocalInvocationIndex] = 0;
   
    // Clever way to find total sum over all bins
    for (uint cutoff = (histogramBinCount >> 1); cutoff > 0; cutoff >>= 1) {
        if (uint(gl_LocalInvocationIndex) < cutoff) {
            sharedHistogram[gl_LocalInvocationIndex] += 
                sharedHistogram[gl_LocalInvocationIndex + cutoff];
        }

        barrier();
    }

    if (gl_LocalInvocationIndex == 0u) {
        float weightedLogAverage = (sharedHistogram[0] / max(pushConstants.pixelCount - float(countForThisBin), 1.0)) - 1.0;

        float weightedAvgLum = exp2((weightedLogAverage / 254.0 * pushConstants.logLuminanceRange) + pushConstants.logLuminanceMin);
        if (isnan(weightedAvgLum) || isinf(weightedAvgLum))
            weightedAvgLum = 1.0;

        // The new stored value will be interpolated using the last frames value
        // to prevent sudden shifts in the exposure.
        float lumLastFrame = imageLoad(exposureImage, ivec2(0, 0)).r;
        //lumLastFrame = 1.0;
        float adaptedLum = lumLastFrame + (weightedAvgLum - lumLastFrame) * pushConstants.timeCoefficient;
        if (isinf(adaptedLum) || isnan(adaptedLum))
            adaptedLum = 1.0;
    
        imageStore(exposureImage, ivec2(0), vec4(adaptedLum, 0.0, 0.0, 0.0));
    }

}