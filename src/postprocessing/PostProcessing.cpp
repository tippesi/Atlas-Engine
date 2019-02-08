#include "PostProcessing.h"

namespace Atlas {

	namespace PostProcessing {

		PostProcessing::PostProcessing() {

			exposure = 1.0f;
			saturation = 1.0f;

			filmicTonemapping = false;

			vignette = nullptr;
			chromaticAberration = nullptr;

		}

	}

}