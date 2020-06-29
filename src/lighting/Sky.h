#ifndef AE_SKY_H
#define AE_SKY_H

#include "../System.h"
#include "EnvironmentProbe.h"
#include "DirectionalLight.h"

namespace Atlas {

	namespace Lighting {

		class Sky {

		public:
			Sky();

			DirectionalLight* sun = nullptr;

			EnvironmentProbe* probe = nullptr;

		};

	}

}


#endif