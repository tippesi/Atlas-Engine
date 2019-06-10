#include "OceanNode.h"

namespace Atlas {

	namespace Ocean {

		OceanNode::OceanNode(vec2 location, float sideLength, float height, int32_t LoD,
			int32_t LoDCount, int32_t LoDMultiplier, ivec2 parentIndex, ivec2 localIndex) :
			location(location), sideLength(sideLength), height(height), LoD(LoD), LoDCount(LoDCount),
			LoDMultiplier(LoDMultiplier), globalIndex(parentIndex + localIndex), localIndex(localIndex) {

			LoDImageOffset = (int32_t)powf(2.0f, (float)(LoDCount - LoD - 1));

		}

		void OceanNode::CheckNeighbourLoD(Common::Image8& LoDImage) {

			ivec2 imageOffset = globalIndex * LoDImageOffset;
			ivec2 imageArea = ivec2(LoDImage.width, LoDImage.height) / LoDMultiplier;

			ivec2 offsets[] = { ivec2(imageOffset.x - 1, imageOffset.y),
				ivec2(imageOffset.x, imageOffset.y - 1),
				ivec2(imageOffset.x + imageArea.x, imageOffset.y),
				ivec2(imageOffset.x, imageOffset.y + imageArea.y) };

			float neighbourLoD[4];

			for (int32_t i = 0; i < 4; i++) {
				if (offsets[i].x < 0 || offsets[i].y < 0 ||
					offsets[i].x >= LoDImage.width ||
					offsets[i].y >= LoDImage.height) {
					neighbourLoD[i] = 0.0f;
					continue;
				}
				auto sample = LoDImage.Sample(offsets[i].x, offsets[i].y);
				neighbourLoD[i] = (LoD - sample.x) > 0 ? (float)(LoD - sample.x) : 0.0f;
			}

			leftLoDStitch = powf(2.0f, neighbourLoD[0]);
			topLoDStitch = powf(2.0f, neighbourLoD[1]);
			rightLoDStitch = powf(2.0f, neighbourLoD[2]);
			bottomLoDStitch = powf(2.0f, neighbourLoD[3]);

		}

		void OceanNode::Update(Camera* camera, std::vector<float>& LoDDistances,
			std::vector<OceanNode*>& leafList, Common::Image8& LoDImage) {

			float calcHeight = 0.0f;

			auto cameraLocation = camera->thirdPerson ? camera->location -
				camera->direction * camera->thirdPersonDistance : camera->location;

			if (cameraLocation.y > height) {
				calcHeight = height;
			}
			else {
				if (cameraLocation.y < 0.0f) {
					calcHeight = 0.0f;
				}
				else {
					calcHeight = camera->location.y;
				}
			}

			// We should check every corner and the middle of the node to get the minimal distance
			auto minDistance = glm::distance(cameraLocation, 
				vec3(location.x + sideLength / 2.0f, calcHeight, location.y + sideLength / 2.0f));

			for (int32_t x = 0; x < 4; x++) {
				for (int32_t y = 0; y < 4; y++) {
					minDistance = glm::min(minDistance, glm::distance(cameraLocation,
						vec3(location.x + (float)x * sideLength * 0.5f, calcHeight,
							 location.y + (float)y * sideLength * 0.5f)));
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

			for (auto& child : children) {
				child.Update(camera, LoDDistances,
					leafList, LoDImage);
			}

			if (!children.size()) {

				// Fill the lod image
				ivec2 imageOffset = globalIndex * LoDImageOffset;
				ivec2 imageArea = ivec2(LoDImage.width, LoDImage.height) / LoDMultiplier;

				for (int32_t y = 0; y < imageArea.y; y++) {
					for (int32_t x = 0; x < imageArea.x; x++) {
						auto offset = imageOffset + ivec2(x, y);
						auto index = offset.y * LoDImage.width + offset.x;
						LoDImage.data[index] = (uint8_t)LoD;
					}
				}

				leafList.push_back(this);
			}

		}

		void OceanNode::CreateChildren() {

			for (int32_t x = 0; x < 2; x++) {
				for (int32_t z = 0; z < 2; z++) {
					children.push_back(OceanNode(vec2(location.x + (float)x * sideLength / 2.0f, location.y + (float)z * sideLength / 2.0f),
						sideLength / 2.0f, height, LoD + 1, LoDCount, LoDMultiplier * 2, globalIndex * 2, ivec2(x, z)));
				}
			}

		}

	}

}