#include "RandomHelper.h"


namespace Atlas {

    namespace Common {

        // https://en.wikipedia.org/wiki/Permuted_congruential_generator
        static uint64_t       state = 0x4d595df4d0f33173;
        static uint64_t const multiplier = 6364136223846793005u;
        static uint64_t const increment = 1442695040888963407u;

        inline uint32_t rotr32(uint32_t x, unsigned r) {
            return x >> r | x << (-r & 31u);
        }

        inline uint32_t pcg32(void) {
            uint64_t x = state;
            unsigned count = (unsigned)(x >> 59u);

            state = x * multiplier + increment;
            x ^= x >> 18;
            return rotr32((uint32_t)(x >> 27u), count);
        }

        void pcg32_init(uint64_t seed) {
            state = seed + increment;
            (void)pcg32();
        }

        std::random_device Random::randDevice;
        std::mt19937 Random::generator(Random::randDevice());
        std::uniform_real_distribution<float> Random::canonicalUniformDistr(0.0f, 0.99999f);

        void Random::Init() {

            pcg32_init(SampleUniformFloat());

        }

        float Random::SampleUniformFloat() {

            return canonicalUniformDistr(generator);

        }

        float Random::SampleFastUniformFloat() {

            return float(pcg32()) / float(0xFFFFFFFF) * 0.999999f;

        }

        float Random::SampleFastUniformFloat(float min, float max) {

            return float(pcg32()) / float(0xFFFFFFFF) * (max - min) + min;

        }

        uint32_t Random::SampleUniformInt(uint32_t min, uint32_t max) {

            return std::min(std::max(uint32_t(SampleFastUniformFloat(float(min), float(max) + 1.0f)), min), max);

        }

    }

}