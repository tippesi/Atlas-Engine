#include "OceanNode.h"

namespace Atlas {

	namespace Ocean {

		OceanNode::OceanNode(vec2 location, float sideLength, float height, int32_t LoD,
			int32_t LoDCount, ivec2 parentIndex, ivec2 localIndex) : location(location),
			sideLength(sideLength), height(height), LoD(LoD), LoDCount(LoDCount), 
			globalIndex(parentIndex + localIndex), localIndex(localIndex) {

		}

		void OceanNode::Update(Camera* camera, std::vector<float>& LoDDistances,
			std::vector<OceanNode*>& renderList, Common::Image8& LoDImage) {

			float calcHeight = 0.0f;

			if (camera->location.y > height) {
				calcHeight = height;
			}
			else {
				if (camera->location.y < 0.0f) {
					calcHeight = 0.0f;
				}
				else {
					calcHeight = camera->location.y;
				}
			}

			// We should check every corner and the middle of the node to get the minimal distance
			auto minDistance = glm::distance(camera->location, 
				vec3(location.x + sideLength / 2.0f, calcHeight, location.y + sideLength / 2.0f));

			for (int32_t x = 0; x < 2; x++) {
				for (int32_t y = 0; y < 2; y++) {
					minDistance = glm::min(minDistance, glm::distance(camera->location,
						vec3(location.x + (float)x * sideLength, calcHeight, location.y + (float)y * sideLength)));
				}
			}

			if (children.size()) {
				if (LoDDistances[LoD] <= minDistance) {
					children.clear();
				}
			}
			else {
				if (LoDCount - 1 > LoD) {
					if (LoDDistances[LoD] > minDistance) {
						CreateChildren();
					}
				}
			}

			for (auto& child : children)
				child.Update(camera, LoDDistances,
					renderList, LoDImage);

			if (!children.size()) {
				renderList.push_back(this);
			}

		}

		void OceanNode::CreateChildren() {

			for (int32_t x = 0; x < 2; x++) {
				for (int32_t z = 0; z < 2; z++) {
					children.push_back(OceanNode(vec2(location.x + (float)x * sideLength / 2.0f, location.y + (float)z * sideLength / 2.0f),
						sideLength / 2.0f, height, LoD + 1, this->LoDCount, globalIndex, ivec2(x, z)));
				}
			}

		}

	}

}