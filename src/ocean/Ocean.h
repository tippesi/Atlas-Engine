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
			Ocean() {}

			Ocean(int32_t LoDCount, float size, vec3 translation = vec3(0.0f),
				int32_t N = 512, int32_t L = 4000);

			void Update(Camera* camera);

			/**
			 * Sets the distance of a specific level of detail.
			 * @param LoD The level of detail to be set in range of (0,LoDCount-1)
			 * @param distance The distance where the level of details should begin
			 */
			void SetLoDDistance(int32_t LoD, float distance);

			std::vector<OceanNode*> GetRenderList();

			OceanSimulation simulation;
			Texture::Texture2D rippleTexture;

			vec3 translation = vec3(0.0f);

			float displacementScale = 4.0f;
			float choppynessScale = 4.0f;

			float tiling = 32.0f;

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