#ifndef AE_SKY_H
#define AE_SKY_H

#include "../System.h"
#include "Skybox.h"

namespace Atlas {

	namespace Lighting {

		class Sky {

		public:
			Sky();

			Skybox* skybox;

		};

	}

}


#endif