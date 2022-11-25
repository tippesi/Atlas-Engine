#ifndef AE_CHROMATICABBERATION_H
#define AE_CHROMATICABBERATION_H

#include "../System.h"

namespace Atlas {

	namespace PostProcessing {

		class ChromaticAberration {

		public:
			ChromaticAberration() = default;

			///
			/// \param strength
			/// \param colorsReversed
			ChromaticAberration(float strength, bool colorsReversed = false) :
					strength(strength), colorsReversed(colorsReversed), enable(true) {};

			bool enable = false;

			float strength;
			bool colorsReversed;

		};

	}

}


#endif