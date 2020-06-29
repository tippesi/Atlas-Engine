#ifndef AE_POSTPROCESSING_H
#define AE_POSTPROCESSING_H

#include "../System.h"

#include "Vignette.h"
#include "ChromaticAberration.h"
#include "Sharpen.h"

namespace Atlas {

	namespace PostProcessing {

		class PostProcessing {

		public:
			float saturation = 1.0f;

			bool filmicTonemapping = false;
			bool taa = true;

			Vignette* vignette = nullptr;
			ChromaticAberration* chromaticAberration = nullptr;
			Sharpen* sharpen = nullptr;

		};

	}

}

#endif