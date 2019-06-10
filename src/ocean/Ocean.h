#ifndef AE_OCEAN_H
#define AE_OCEAN_H

#include "../System.h"
#include "../Camera.h"

#include "OceanState.h"
#include "OceanNode.h"
#include "OceanSimulation.h"

#include <vector>

namespace Atlas {

	namespace Ocean {

		class Ocean : public OceanNode {

		public:
			Ocean(int32_t LoDCount, float size, float height = 0.0f);

			void Update(Camera* camera);

			/**
			 * Sets the distance of a specific level of detail.
			 * @param LoD The level of detail to be set in range of (0,LoDCount-1)
			 * @param distance The distance where the level of details should begin
			 */
			void SetLoDDistance(int32_t LoD, float distance);

			std::vector<OceanNode*> GetRenderList();

			OceanSimulation simulation;

			float displacementScale = 4.0f;
			float choppynessScale = 4.0f;

			float tiling = 32.0f;

			float height;

		private:
			void SortNodes(std::vector<OceanNode*>& nodes, vec3 cameraLocation);

			std::vector<OceanNode*> renderList;

			Common::Image8 LoDImage;

			std::vector<float> LoDDistances;

			float size;

		};

	}

}


#endif