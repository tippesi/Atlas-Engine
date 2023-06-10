#ifndef AE_PIECEWISE_H
#define AE_PIECEWISE_H

#include "../System.h"

#include <vector>
#include <stdint.h>

namespace Atlas {

    namespace Common {

        class Piecewise1D {

        public:
            Piecewise1D() = default;

            /**
             * Initializes the piecewise distribution with a probability density function
             * @param pdf The probability density provided in form of a vector
             */
            Piecewise1D(const std::vector<float>& pdf);

            /**
             * Samples the piecewise distribution
             * @param pdf The probability of that sample
             * @return The interpolated sample
             * @note The return value will be in the range [pdf[offset], pdf[offset + 1]]
             */
            float Sample(float& pdf);

            /**
             * Samples the piecewise distribution
             * @param pdf the probability of the sample
             * @param offset The index of the sample
             * @return The interpolated sample
             * @note The return value will be in the range [pdf[offset], pdf[offset + 1]]
             */
            float Sample(float& pdf, int32_t& offset);
                
            uint32_t count = 0;
            float sum = 0.0f;

            std::vector<float> pdf;
            std::vector<float> cdf;

        private:
            int32_t BinarySearch(float pdf);

        };

        class Piecewise2D {

        public:
            Piecewise2D() = default;

            Piecewise2D(std::vector<float> data, int32_t nu, int32_t nv);

            /**
             * Samples the piecewise distribution
             * @param pdf The probability of that sample
             * @return The interpolated coordinate which lies between samples
             */
            glm::vec2 Sample(float& pdf);

            /**
             * Returns the probability for a certain sample coordinate
             * @param uv The sample coordinate
             * @return The probability for the sample
             */
            float Pdf(glm::vec2 uv);

        private:
            Piecewise1D marginalDistribution;
            std::vector<Piecewise1D> conditionalDistributions;

        };

    }

}


#endif