#ifndef AE_OCEAN_H
#define AE_OCEAN_H

#include "../System.h"
#include "../Camera.h"

#include "OceanState.h"
#include "OceanNode.h"

namespace Atlas {

	namespace Ocean {

		class Ocean : public OceanNode {

		public:
			Ocean(int32_t maxDepth, float size);

			void Update(Camera* camera);

		};

	}

}


#endif