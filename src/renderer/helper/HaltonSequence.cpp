#include "HaltonSequence.h"

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			std::vector<float> HaltonSequence::Generate(int32_t base, int32_t count) {

				std::vector<float> sequence;

				for (int32_t i = 0; i < count; i++) {
					sequence.push_back(Halton(base, i));
				}

				return sequence;

			}

			std::vector<vec2> HaltonSequence::Generate(int32_t baseX, int32_t baseY, int32_t count) {

				std::vector<vec2> sequence;

				for (int32_t i = 0; i < count; i++) {
					sequence.push_back(vec2(Halton(baseX, i), Halton(baseY, i)));
				}

				return sequence;
			
			}

			float HaltonSequence::Halton(int32_t base, int32_t index) {

				auto f = 1.0f;
				auto r = 0.0f;

				while (index > 0) {
					f = f / (float)base;
					r = r + f * (index % base);
					index /= base;
				}

				return r;

			}

		}

	}

}