#ifndef AE_FRUSTUM_H
#define AE_FRUSTUM_H

#include "../System.h"

namespace Atlas {

	namespace Common {

		class Frustum {

		public:
			Frustum() {}

			void Resize();

			void Update();

		};

	}

}

#endif