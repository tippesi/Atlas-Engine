#ifndef AE_KERNEL_H
#define AE_KERNEL_H

#include "System.h"
#include <vector>

namespace Atlas {

    /**
     * Manages convolution matrices.
     */
    class Kernel {

    public:
        /**
         * Constructs a Kernel.
         */
        Kernel();

        /**
         * Calculates a gaussian convolution matrix.
         * @param sigma The sigma used for the gaussian function.
         * @param size The diameter of the convolution matrix.
         * @note The size argument must always be an odd number, with size > 1.
         * @remark See <a href="https://en.wikipedia.org/wiki/Gaussian_blur">Gaussian blur</a> for more.
         */
        void CalculateGaussianFilter(float sigma, uint32_t size);

        /**
         * Calculates a box convolution matrix.
         * @param size The diameter of the convolution matrix.
         * @note The size argument must always be an odd number, with size > 1.
         * @remark See <a href="https://en.wikipedia.org/wiki/Box_blur">Box blur</a> for more.
         */
        void CalculateBoxFilter(uint32_t size);

        /**
         * Sets the convolution matrix.
         * @param weights The weights for the convolution matrix.
         * @param offsets The offsets for the convolution matrix.
         * @note The size of the weights and the offsets should be odd in every dimension.
         */
        void Set(std::vector<std::vector<float>> &weights, std::vector<std::vector<ivec2>> &offsets);

        /**
         * Returns the convolution matrix.
         * @param weights A pointer which will contain the weights.
         * @param offsets A pointer which will contain the offsets.
         * @note The input pointers shouldn't have anything allocated and shouldn't be deleted later on.
         */
        void Get(std::vector<std::vector<float>> *&weights, std::vector<std::vector<ivec2>> *&offsets);

        /**
         * Returns the linearized convolution matrix.
         * @param weights A pointer which will contain the weights.
         * @param offsets A pointer which will contain the offsets.
         * @note The input pointers shouldn't have anything allocated and shouldn't be deleted later on.
         * @remark This method should only be used if the filter is separable
         * (explanation <a href="https://en.wikipedia.org/wiki/Separable_filter">here</a>) and if the sampling
         * supports bilinear interpolation. Linearized means that the convolution matrix is reduced to a
         * one dimensional array. The filter can then be applied in two passes (horizontal, vertical). Only one side
         * of the original convolution matrix is included in the linearized matrix with the origin being at index 0.
         * Additionally the samples of the linearized matrix are cut by half by weighting the offsets in such a
         * way that the new samples are always between two of the original samples.
         */
        void GetLinearized(std::vector<float> *&weights, std::vector<float> *&offsets);

    private:
        float Gaussian(float x, float y, float mean, float sigma);

        std::vector<std::vector<float>> weights;
        std::vector<std::vector<ivec2>> offsets;

        std::vector<float> weightsLinearized;
        std::vector<float> offsetsLinearized;

        bool changed;

    };

}

#endif