#pragma once

#include "../System.h"

#include <vector>

namespace Atlas {

    namespace Audio {

        struct AudioReverbSettings {

            // How long the reverb rings, 0 a small room, 1 a large hall or cave
            float roomSize = 0.5f;
            // How quickly the high frequencies die away
            float damping = 0.5f;
            // Volume of the reverb in the mix, 0 turns it off
            float wet = 0.0f;
            // Stereo width of the reverb
            float width = 1.0f;

        };

        /**
         * A stereo reverb of parallel comb filters and serial allpass filters (Freeverb), with the delays
         * scaled to the sample rate. The settings fade towards the ones set, changing rooms doesn't click.
         */
        class AudioReverb {

        public:
            explicit AudioReverb(int32_t sampleRate = 48000);

            // Interleaved stereo frames, the reverb of the input is added to the output
            void Process(const float* input, float* output, int32_t frameCount);

            AudioReverbSettings target;

        private:
            struct Comb {
                std::vector<float> buffer;
                size_t index = 0;
                float store = 0.0f;
            };

            struct Allpass {
                std::vector<float> buffer;
                size_t index = 0;
            };

            std::vector<Comb> combs[2];
            std::vector<Allpass> allpasses[2];

            AudioReverbSettings current;
            bool silent = false;

        };

    }

}
