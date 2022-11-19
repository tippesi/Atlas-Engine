#include "Piecewise.h"
#include "RandomHelper.h"

#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace Atlas {

	namespace Common {

		Piecewise1D::Piecewise1D(const std::vector<float>& pdf) :
			count(uint32_t(pdf.size())), pdf(pdf) {

			// sum will be the normalization constant
			for (auto ele : pdf) {
				cdf.push_back(sum);
				sum += (ele / float(count));
			}
			cdf.push_back(sum);

			for (auto& ele : cdf) {
				ele /= sum;
			}

		}

		float Piecewise1D::Sample(float& pdf) {

			int32_t offset = 0;
			return Sample(pdf, offset);

		}

		float Piecewise1D::Sample(float& pdf, int32_t& offset) {

			auto x = Random::SampleFastUniformFloat();
			offset = BinarySearch(x);

			auto dx = x - cdf[offset];
			auto diff = cdf[offset + 1] - cdf[offset];
			if (diff > 0.0f)
				dx /= diff;

			pdf = this->pdf[offset] / sum;

			return (float(offset) + dx) / float(count);

		}

		int32_t Piecewise1D::BinarySearch(float x) {
			int l = 0, r = int(count);
			while (l <= r) {
				int m = l + (r - l) / 2;
				int k = m + 1 > int(count) ? m : m + 1;

				auto cm = cdf[m];
				auto ck = cdf[k];

				if (cdf[m] <= x && x <= cdf[k]) {
					return m;
				}
				if (cdf[m] < x)
					l = m + 1;
				else
					r = m - 1;
			}
			return -1;
		}

		Piecewise2D::Piecewise2D(std::vector<float> data, int32_t nu, int32_t nv) {

			for (int32_t v = 0; v < nv; v++) {
				auto offset = data.begin() + v * nu;
				auto end = offset + nu;
				std::vector<float> sub(offset, end);
				conditionalDistributions.emplace_back(Piecewise1D{ sub });
			}

			std::vector<float> marginal;
			for (auto& conditional : conditionalDistributions)
				marginal.push_back(conditional.sum);
			marginalDistribution = Piecewise1D(marginal);

		}

		glm::vec2 Piecewise2D::Sample(float& pdf) {

			auto cond = 0;
			auto marginalPdf = 0.0f, conditionalPdf = 0.0f;
			auto v = marginalDistribution.Sample(marginalPdf, cond);
			auto u = conditionalDistributions[cond].Sample(conditionalPdf);

			pdf = marginalPdf * conditionalPdf;
			return glm::vec2(u, v);

		}

		float Piecewise2D::Pdf(glm::vec2 uv) {

			auto cn = int32_t(conditionalDistributions[0].count);
			auto mn = int32_t(marginalDistribution.count);

			auto iu = glm::clamp(int32_t(uv.x * float(cn)), 0, cn - 1);
			auto iv = glm::clamp(int32_t(uv.y * float(mn)), 0, mn - 1);

			return conditionalDistributions[iv].pdf[iu] / marginalDistribution.count;

		}

	}

}