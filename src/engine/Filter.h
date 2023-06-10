#ifndef AE_KERNEL_H
#define AE_KERNEL_H

#include "System.h"
#include <vector>

namespace Atlas {

    /**
     * Manages convolution matrices.
     */
    class Filter {

    public:
        /**
         * Constructs a Filter.
         */
        Filter() = default;

        /**
         * Calculates a gaussian convolution matrix.
         * @param sigma The sigma used for the gaussian function.
         * @param size The diameter of the convolution matrix.
         * @note This doesn't create a filter of size size, but creates
         * a filter with the size 2 * size + 1, with mean = size
         * @remark See <a href="https://en.wikipedia.org/wiki/Gaussian_blur">Gaussian blur</a> for more.
         */
        void CalculateGaussianFilter(float sigma, uint32_t size);

        /**
         * Calculates a box convolution matrix.
         * @param size The diameter of the convolution matrix.
         * @note This doesn't create a filter of size size, but creates
         * a filter with the size 2 * size + 1, with mean = size
         * @remark See <a href="https://en.wikipedia.org/wiki/Box_blur">Box blur</a> for more.
         */
        void CalculateBoxFilter(uint32_t size);

        /**
         * Sets the convolution matrix.
         * @param weights The weights for the convolution matrix.
         * @param offsets The offsets for the convolution matrix.
         * @param separable Whether or not the convolution matrix is separable
         * @note The size of the weights and the offsets should be odd in every dimension.
         */
        void Set(const std::vector<std::vector<float>>& weights, 
            const std::vector<std::vector<ivec2>>& offsets, bool separable = false);

        /**
         * Returns the convolution matrix.
         * @param weights A pointer to a weight matrix which will contain the weights.
         * @param offsets A pointer to an offset matrix which will contain the offsets.
         * @note The input pointers should point to allocated memory.
         */
        void Get(std::vector<std::vector<float>>* weights, 
            std::vector<std::vector<ivec2>>* offsets) const;

        /**
         * Returns the linearized convolution matrix.
         * @param weights A pointer to a weight vector which will contain the weights.
         * @param offsets A pointer to a offset vector which will contain the offsets.
         * @param linearReduction Should be set to true if linear sampling is supported by the hardware.
         * @note The input pointers should point to allocated memory.
         * @remark This method should only be used if the filter is separable
         * (explanation <a href="https://en.wikipedia.org/wiki/Separable_filter">here</a>) and if the sampling
         * supports bilinear interpolation. Linearized means that the convolution matrix is reduced to a
         * one dimensional array. The filter can then be applied in two passes (horizontal, vertical). Only one side
         * of the original convolution matrix is included in the linearized matrix with the origin being at index 0.
         * Additionally the samples of the linearized matrix are cut by half by weighting the offsets in such a
         * way that the new samples are always between two of the original samples.
         */
        void GetLinearized(std::vector<float>* weights, std::vector<float>* offsets, bool linearReduction = true);

        /**
         * Returns whether or not the convolution matrix is separable
         * @return Whether or not the convolution matrix is separable
         */
        bool IsSeparable() const;

    private:
        float Gaussian(float x, float y, float mean, float sigma);

        std::vector<std::vector<float>> weights;
        std::vector<std::vector<ivec2>> offsets;

        std::vector<float> weightsLinearized;
        std::vector<float> offsetsLinearized;

        bool changed = false;
        bool separable = false;

    };

}

#endif