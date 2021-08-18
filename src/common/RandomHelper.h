#ifndef AE_RANDOM_H
#define AE_RANDOM_H

#include <random>

namespace Atlas {

	namespace Common {

		class Random {

		public:
			static void Init();

			static float CanonicalUniform();

			static float FastCanonicalUniform();

		private:
			static std::random_device randDevice;
			static std::mt19937 generator;
			static std::uniform_real_distribution<float> canonicalUniformDistr;

		};

	}

}

#endif