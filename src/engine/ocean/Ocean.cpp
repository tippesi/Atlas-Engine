#include "Ocean.h"

#include <algorithm>

namespace Atlas {

	namespace Ocean {

		Ocean::Ocean(int32_t LoDCount, float size, vec3 translation, int32_t N,
			int32_t L) : size(size), translation(translation), simulation(N, L) {

			int32_t imageSize = (int32_t)powf(2, (float)LoDCount - 1.0f);
			LoDImage = Common::Image<uint8_t>(imageSize, imageSize, 1);

			location = vec2(-size / 2.0f);
			sideLength = size;

			LoD = 0;
			this->LoDCount = LoDCount;
			LoDMultiplier = 1;

			LoDDistances.resize((size_t)LoDCount);

			auto distance = size;

			for (int32_t i = 0; i < LoDCount; i++) {
				distance /= 2.0f;
				LoDDistances[i] = distance;
			}

		}

		void Ocean::Update(Camera* camera, float deltaTime) {

			if (!enable) return;

			simulation.choppinessScale = choppynessScale;
			simulation.displacementScale = displacementScale;
			simulation.tiling = tiling;

			simulation.Update(deltaTime);

			std::vector<OceanNode*> leafs;

			OceanNode::Update(camera, translation, LoDDistances, leafs, LoDImage);

			renderList.clear();

			for (auto node : leafs) {
				auto aabb = Volume::AABB(
					vec3(node->location.x, -50.0, node->location.y) + translation,
					vec3(node->location.x + node->sideLength, 50.0f,
						node->location.y + node->sideLength) + translation
				);

				if (camera->frustum.Intersects(aabb))
					renderList.push_back(node);
			}

			for (auto node : renderList) {
				node->CheckNeighbourLoD(LoDImage);
			}

			auto cameraLocation = camera->GetLocation();

			// Sort the list to render from front to back
			SortNodes(renderList, cameraLocation);

		}

		void Ocean::SetLoDDistance(int32_t LoD, float distance) {

			if (LoD >= 0 && LoD < LoDCount) {
				LoDDistances[LoD] = distance;
			}

		}

		std::vector<OceanNode*> Ocean::GetRenderList() {

			return renderList;

		}

		void Ocean::SortNodes(std::vector<OceanNode*>& nodes, vec3 cameraLocation) {

			std::sort(nodes.begin(), nodes.end(),
				[=](OceanNode* node1, OceanNode* node2) -> bool {

				auto distance1 = glm::distance(cameraLocation,
					vec3(node1->location.x + node1->sideLength / 2.0f,
					0.0f, node1->location.y + node1->sideLength / 2.0f));
				auto distance2 = glm::distance(cameraLocation,
					vec3(node2->location.x + node2->sideLength / 2.0f,
					0.0f, node2->location.y + node2->sideLength / 2.0f));

				return distance1 < distance2;

				});

		}

	}

}