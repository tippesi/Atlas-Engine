#ifndef AE_POSTPROCESSING_H
#define AE_POSTPROCESSING_H

#include "../System.h"
#include "Vignette.h"
#include "ChromaticAberration.h"

namespace Atlas {

	namespace PostProcessing {

		class PostProcessing {

		public:
			///
			PostProcessing();

			float exposure;
			float saturation;

			bool filmicTonemapping;

			Vignette* vignette;
			ChromaticAberration* chromaticAberration;

		};

	}

}

#endif