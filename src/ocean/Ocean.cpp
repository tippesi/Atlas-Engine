#include "Ocean.h"

#include <algorithm>

namespace Atlas {

	namespace Ocean {

		Ocean::Ocean(int32_t LoDCount, float size, float height) :
			size(size), height(height),	simulation(512, 4000) {

			simulation.ComputeSpectrum();

			int32_t imageSize = (int32_t)powf(2, (float)LoDCount - 1.0f);
			LoDImage = Common::Image8(imageSize, imageSize, 1);

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

		void Ocean::Update(Camera* camera) {

			simulation.Compute();

			std::vector<OceanNode*> leafs;

			OceanNode::Update(camera, LoDDistances, leafs, LoDImage);

			renderList.clear();

			// TODO: Check every node against the camera
			for (auto node : leafs) {
				auto aabb = Common::AABB(
					vec3(node->location.x, height, node->location.y),
					vec3(node->location.x + node->sideLength, height + 100.0f,
						node->location.y + node->sideLength)
				);

				if (camera->frustum.IsVisible(aabb))
					renderList.push_back(node);
			}

			for (auto node : renderList) {
				node->CheckNeighbourLoD(LoDImage);
			}

			// Sort the list to render from front to back
			SortNodes(renderList, camera);

		}

		void Ocean::SetLoDDistance(int32_t LoD, float distance) {

			if (LoD >= 0 && LoD < LoDCount) {
				LoDDistances[LoD] = distance;
			}

		}

		std::vector<OceanNode*> Ocean::GetRenderList() {

			return renderList;

		}

		void Ocean::SortNodes(std::vector<OceanNode*>& nodes, Camera* camera) {

			std::sort(nodes.begin(), nodes.end(),
				[=](OceanNode* node1, OceanNode* node2) -> bool {

				auto distance1 = glm::distance(camera->location,
					vec3(node1->location.x + node1->sideLength / 2.0f,
					0.0f, node1->location.y + node1->sideLength / 2.0f));
				auto distance2 = glm::distance(camera->location,
					vec3(node2->location.x + node2->sideLength / 2.0f,
					0.0f, node2->location.y + node2->sideLength / 2.0f));

				return distance1 < distance2;

				});

		}

	}

}