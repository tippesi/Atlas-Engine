#include "Ocean.h"

namespace Atlas {

	namespace Ocean {

		Ocean::Ocean(int32_t LoDCount, float size, float height) :
			size(size), height(height),	simulation(512, 4000) {

			simulation.SetState(1.0f, vec2(1.0f, 1.0f), 60.0f, 0.07f);

			int32_t imageSize = (int32_t)powf(2, (float)LoDCount - 1.0f);
			LoDImage = Common::Image8(imageSize, imageSize, 1);

			location = vec2(-size / 2.0f);
			sideLength = size;

			LoD = 0;
			this->LoDCount = LoDCount;

			LoDDistances.resize((size_t)LoDCount);

			float distance = size;

			for (int32_t i = 0; i < LoDCount; i++) {
				LoDDistances[i] = distance;
				distance /= 2.0f;
			}

		}

		void Ocean::Update(Camera* camera) {

			simulation.Compute();

			renderList.clear();

			OceanNode::Update(camera, LoDDistances, renderList, LoDImage);

		}

		void Ocean::SetLoDDistance(int32_t LoD, float distance) {

			if (LoD >= 0 && LoD < LoDCount) {
				LoDDistances[LoD] = distance;
			}

		}

		std::vector<OceanNode*> Ocean::GetRenderList() {

			return renderList;

		}

	}

}