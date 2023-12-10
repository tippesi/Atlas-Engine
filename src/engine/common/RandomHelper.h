#pragma once

#include <random>

namespace Atlas {

    namespace Common {

        class Random {

        public:
            static void Init();

            static float SampleUniformFloat();

            static float SampleFastUniformFloat();

            static float SampleFastUniformFloat(float min, float max);

            static uint32_t SampleUniformInt(uint32_t min, uint32_t max);

        private:
            static std::random_device randDevice;
            static std::mt19937 generator;
            static std::uniform_real_distribution<float> canonicalUniformDistr;

        };

    }

}