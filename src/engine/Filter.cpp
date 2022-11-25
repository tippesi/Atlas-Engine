#include "Filter.h"

namespace Atlas {

	void Filter::CalculateGaussianFilter(float sigma, uint32_t size) {

		size = 2 * size + 1;
		float weightSum = 0.0f;
		float mean = float(size - 1) / 2.0f;
		ivec2 offset((size - 1) / 2);

		weights.resize(size);
		offsets.resize(size);

		for (uint32_t x = 0; x < size; x++) {
			weights[x].resize(size);
			offsets[x].resize(size);
			for (uint32_t y = 0; y < size; y++) {
				weights[x][y] = Gaussian((float) x, (float) y, mean, sigma);
				offsets[x][y] = ivec2(x, y) - offset;
				weightSum += weights[x][y];
			}
		}

		for (uint32_t x = 0; x < size; x++) {
			for (uint32_t y = 0; y < size; y++) {
				weights[x][y] /= weightSum;
			}
		}

		changed = true;
		separable = true;

	}

	void Filter::CalculateBoxFilter(uint32_t size) {

		size = 2 * size + 1;
		float partialWeight = 1.0f / (float) (size * size);
		ivec2 offset((size - 1) / 2);

		weights.resize(size);
		offsets.resize(size);

		for (uint32_t x = 0; x < size; x++) {
			weights[x].resize(size);
			offsets[x].resize(size);
			for (uint32_t y = 0; y < size; y++) {
				weights[x][y] = partialWeight;
				offsets[x][y] = ivec2(x, y) - offset;
			}
		}

		changed = true;
		separable = true;

	}

	void Filter::Set(const std::vector<std::vector<float>> &weights, const std::vector<std::vector<ivec2>> &offsets, bool separable) {

		this->weights = weights;
		this->offsets = offsets;
		this->separable = separable;

		changed = true;

	}

	void Filter::Get(std::vector<std::vector<float>>* weights, std::vector<std::vector<ivec2>>* offsets) const {

		*weights = this->weights;
		*offsets = this->offsets;

	}

	void Filter::GetLinearized(std::vector<float>* weights, std::vector<float>* offsets, bool bilinearReduction) {

		if (!changed) {
			*weights = weightsLinearized;
			*offsets = offsetsLinearized;
			return;
		}

		uint32_t size = (uint32_t) this->weights.size();

		std::vector<float> oneDimensionalWeights(size);
		std::vector<float> oneDimensionalOffsets(size);

		// Shrink the 2-dimensional kernel to a 1-dimensional one.
		for (uint32_t x = 0; x < size; x++) {
			oneDimensionalOffsets[x] = (float) this->offsets[x][0].x;
			for (uint32_t y = 0; y < size; y++) {
				oneDimensionalWeights[x] += this->weights[x][y];
			}
		}

		uint32_t linearizedSize = (size + 3) / 4;
		uint32_t arrayOffset = (size + 1) / 2;

		weightsLinearized.resize(linearizedSize);
		offsetsLinearized.resize(linearizedSize);

		if (bilinearReduction) {

			// Now weight the offsets in such a way that we only need half of the weights/offsets
			// This is just useful when sampling from a texture that supports bilinear sampling.
			weightsLinearized[0] = oneDimensionalWeights[arrayOffset - 1];
			offsetsLinearized[0] = oneDimensionalOffsets[arrayOffset - 1];

			int32_t index = 1;

			for (uint32_t x = arrayOffset; x < size; x += 2) {
				float linearWeight = oneDimensionalWeights[x] + oneDimensionalWeights[x + 1];
				float linearOffset = (oneDimensionalWeights[x] * oneDimensionalOffsets[x] +
					oneDimensionalWeights[x + 1] * oneDimensionalOffsets[x + 1]) / linearWeight;
				weightsLinearized[index] = linearWeight;
				offsetsLinearized[index] = linearOffset;
				index++;
			}

		}
		else {

			weightsLinearized = oneDimensionalWeights;
			offsetsLinearized = oneDimensionalOffsets;

		}

		*weights = weightsLinearized;
		*offsets = offsetsLinearized;

		changed = false;

	}

	bool Filter::IsSeparable() const {

		return separable;

	}

	float Filter::Gaussian(float x, float y, float mean, float sigma) {

		return expf(-0.5f * (powf((x - mean) / sigma, 2.0f) + powf((y - mean) / sigma, 2.0f))) /
			   (2.0f * 3.141592643f * sigma * sigma);

	}

}