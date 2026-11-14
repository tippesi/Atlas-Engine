#include "AudioReverb.h"
#include "../common/MathHelper.h"

#include <algorithm>

namespace Atlas {

    namespace Audio {

        // Delays in samples at 44.1 kHz, the right channel is spread a little
        static constexpr int32_t combTunings[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr int32_t allpassTunings[] = { 556, 441, 341, 225 };
        static constexpr int32_t stereoSpread = 23;
        static constexpr float inputGain = 0.015f;
        static constexpr float allpassFeedback = 0.5f;

        AudioReverb::AudioReverb(int32_t sampleRate) {

            auto scale = float(sampleRate) / 44100.0f;
            for (int32_t channel = 0; channel < 2; channel++) {
                auto spread = channel * stereoSpread;
                for (auto tuning : combTunings) {
                    Comb comb;
                    comb.buffer.resize(size_t(float(tuning + spread) * scale), 0.0f);
                    combs[channel].push_back(comb);
                }
                for (auto tuning : allpassTunings) {
                    Allpass allpass;
                    allpass.buffer.resize(size_t(float(tuning + spread) * scale), 0.0f);
                    allpasses[channel].push_back(allpass);
                }
            }

        }

        void AudioReverb::Process(const float* input, float* output, int32_t frameCount) {

            // A block is a few milliseconds, the settings reach the target within a few hundred
            const float fade = 0.02f;
            current.roomSize += (target.roomSize - current.roomSize) * fade;
            current.damping += (target.damping - current.damping) * fade;
            current.wet += (target.wet - current.wet) * fade;
            current.width += (target.width - current.width) * fade;

            if (current.wet < 0.0001f && target.wet == 0.0f) {
                // The tail of the last room mustn't ring again once the reverb is turned on
                if (!silent) {
                    for (int32_t channel = 0; channel < 2; channel++) {
                        for (auto& comb : combs[channel]) {
                            std::fill(comb.buffer.begin(), comb.buffer.end(), 0.0f);
                            comb.store = 0.0f;
                        }
                        for (auto& allpass : allpasses[channel])
                            std::fill(allpass.buffer.begin(), allpass.buffer.end(), 0.0f);
                    }
                    silent = true;
                }
                return;
            }
            silent = false;

            auto feedback = 0.7f + 0.28f * Helper::Math::Saturate(current.roomSize);
            auto damping = 0.4f * Helper::Math::Saturate(current.damping);
            auto wet1 = current.wet * (0.5f + 0.5f * current.width);
            auto wet2 = current.wet * (0.5f - 0.5f * current.width);

            for (int32_t i = 0; i < frameCount; i++) {
                auto in = (input[i * 2] + input[i * 2 + 1]) * inputGain;

                float result[2];
                for (int32_t channel = 0; channel < 2; channel++) {
                    auto sum = 0.0f;
                    for (auto& comb : combs[channel]) {
                        auto out = comb.buffer[comb.index];
                        comb.store = out * (1.0f - damping) + comb.store * damping;
                        comb.buffer[comb.index] = in + comb.store * feedback;
                        comb.index = (comb.index + 1) % comb.buffer.size();
                        sum += out;
                    }
                    for (auto& allpass : allpasses[channel]) {
                        auto buffered = allpass.buffer[allpass.index];
                        allpass.buffer[allpass.index] = sum + buffered * allpassFeedback;
                        allpass.index = (allpass.index + 1) % allpass.buffer.size();
                        sum = buffered - sum;
                    }
                    result[channel] = sum;
                }

                output[i * 2] += result[0] * wet1 + result[1] * wet2;
                output[i * 2 + 1] += result[1] * wet1 + result[0] * wet2;
            }

        }

    }

}
