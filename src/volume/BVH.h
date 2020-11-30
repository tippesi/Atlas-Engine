#ifndef AE_BVH_H
#define AE_BVH_H

#include "../System.h"
#include "AABB.h"
#include "Ray.h"

#include <vector>
#include <algorithm>

namespace Atlas {

	namespace Volume {

		template <class T> class BVHNode {

		public:
			BVHNode() = default;

			void BuildSAH(std::vector<BVHNode<T>*>& nodes, size_t offset, size_t count, std::vector<AABB>& aabbs,
				std::vector<T>& data, int32_t& nodeCount, int32_t depth, bool isLeftChild);

			void GetIntersection(Ray ray, std::vector<BVHNode<T>>& nodes,
				std::vector<AABB>& aabbs, std::vector<T>& data,
				std::vector<T>& intersections);

			AABB aabb;

			int32_t leftChild = 0;
			int32_t rightChild = 0;

			int32_t dataOffset = 0;
			int32_t dataCount = 0;

			int32_t depth = 0;
			bool isLeftChild = false;

		};

		template <class T> class BVH {

		public:
			BVH() = default;

			/**
			 * @note The contructor works on the input data and sorts it.
			 */
			BVH(std::vector<AABB>& aabbs, std::vector<T>& data);

			std::vector<T> GetIntersection(Ray ray);

			std::vector<BVHNode<T>>& GetTree();

			std::vector<AABB> aabbs;
			std::vector<T> data;
			int32_t maxDepth = 0;

		private:
			std::vector<BVHNode<T>> nodes;

		};

		template <class T>
		BVH<T>::BVH(std::vector<AABB>& aabbs, std::vector<T>& data) {

			if (aabbs.size() != data.size())
				return;

			int32_t nodeCount = 1;

			// We need to use pointers first and copy memory later
			// because when the vector resizes we still work on elements
			// of that vector which leads to errors.
			std::vector<BVHNode<T>*> nodesPointer;

			nodesPointer.push_back(new BVHNode<T>());
			nodesPointer[0]->BuildSAH(nodesPointer, 0, aabbs.size(),
				aabbs, data, nodeCount, 0, true);

			// Copy nodes
			for (auto node : nodesPointer) {
				nodes.push_back(*node);
				maxDepth = glm::max(node->depth, maxDepth);
				delete node;
			}

			// Copy data
			this->aabbs = aabbs;
			this->data = data;

		}

		template <class T>
		std::vector<T> BVH<T>::GetIntersection(Ray ray) {

			std::vector<T> intersections;

			if (nodes.size()) {
				nodes[0].GetIntersection(ray, nodes, aabbs, 
					data, intersections);
			}

			return intersections;

		}

		template <class T>
		std::vector<BVHNode<T>>& BVH<T>::GetTree() {

			return nodes;

		}

		template <class T>
		void BVHNode<T>::BuildSAH(std::vector<BVHNode<T>*>& nodes, size_t offset, size_t count, std::vector<AABB>& aabbs,
			std::vector<T>& data, int32_t& nodeCount, int32_t depth, bool isLeftChild) {

			const float binCount = 64.0f;

			this->depth = depth;
			this->isLeftChild = isLeftChild;

			// Calculate AABB for node
			auto min = vec3(std::numeric_limits<float>::max());
			auto max = vec3(-std::numeric_limits<float>::max());

			for (size_t i = offset; i < offset + count; i++) {
				min = glm::min(min, aabbs[i].min);
				max = glm::max(max, aabbs[i].max);
			}

			aabb = AABB(min, max);

			// Create leaf node
			if (count <= 1) {
				dataOffset = (int32_t)offset;
				dataCount = (int32_t)count;
				return;
			}

			auto dimension = max - min;

			// Calculate cost for current node
			auto minCost = (float)count * (dimension.x * dimension.y +
				dimension.y * dimension.z + dimension.z * dimension.x);

			int32_t bestAxis = -1;
			auto bestSplit = std::numeric_limits<float>::max();

			// Iterate over 3 axises
			for (int32_t i = 0; i < 3; i++) {

				auto start = min[i];
				auto stop = max[i];

				// If the dimension of this axis is to small continue
				if (fabsf(stop - start) < 1e-3)
					continue;

				auto step = (stop - start) / (binCount / ((float)depth + 1.0f));

				// Iterate over all possible splits of the bins
				for (float split = start + step; split < stop - step; split += step) {

					Volume::AABB aabbLeft(vec3(std::numeric_limits<float>::max()),
						vec3(-std::numeric_limits<float>::max()));
					Volume::AABB aabbRight(vec3(std::numeric_limits<float>::max()),
						vec3(-std::numeric_limits<float>::max()));

					// Primitives in left and right bounding boxes
					int32_t primitivesLeft = 0;
					int32_t primitivesRight = 0;

					// Iterate over data and update split bounds
					for (size_t j = offset; j < offset + count; j++) {
						auto& primitveAABB = aabbs[j];

						auto center = 0.5f * (primitveAABB.min + primitveAABB.max);
						auto value = center[i];

						if (value < split) {
							aabbLeft.min = glm::min(aabbLeft.min, aabbs[j].min);
							aabbLeft.max = glm::max(aabbLeft.max, aabbs[j].max);
							primitivesLeft++;
						}
						else {
							aabbRight.min = glm::min(aabbRight.min, aabbs[j].min);
							aabbRight.max = glm::max(aabbRight.max, aabbs[j].max);
							primitivesRight++;
						}
					}

					// We don't want to have useless partitionings
					if (primitivesLeft <= 0 || primitivesRight <= 0)
						continue;

					auto leftDimension = aabbLeft.max - aabbLeft.min;
					auto rightDimension = aabbRight.max - aabbRight.min;

					auto leftSurface = (leftDimension.x * leftDimension.y +
						leftDimension.y * leftDimension.z +
						leftDimension.z * leftDimension.x);

					auto rightSurface = (rightDimension.x * rightDimension.y +
						rightDimension.y * rightDimension.z +
						rightDimension.z * rightDimension.x);

					// Caculate cost for current split
					auto cost = leftSurface * (float)primitivesLeft +
						rightSurface * (float)primitivesRight;

					// Check if cost has improved
					if (cost < minCost) {
						minCost = cost;
						bestAxis = i;
						bestSplit = split;
					}

				}

			}

			// If we haven't found a cost improvement we create a leaf node
			if (bestAxis < 0) {
				dataOffset = (int32_t)offset;
				dataCount = (int32_t)count;
				return;
			}

			// Find split index
			size_t splitIdx = 0;
			for (size_t i = offset; i < offset + count; i++) {
				auto& primitveAABB = aabbs[i];

				auto center = 0.5f * (primitveAABB.min + primitveAABB.max);
				auto value = center[bestAxis];

				if (value < bestSplit) {
					splitIdx++;
				}
			}

			// Sort the data and aabb array based on split in O(n) in-place
			bool leftIncrement = true;
			auto leftIdx = offset;
			auto rightIdx = offset + splitIdx;

			auto carryover = false;
			T carryoverData;
			AABB carryoverAABB;

			// Just one condition needs to be true for the array to be sorted
			while (leftIdx < offset + splitIdx && rightIdx < offset + count) {
				auto& idx = leftIncrement ? leftIdx : rightIdx;
				if (carryover) {
					data[idx] = carryoverData;
					aabbs[idx] = carryoverAABB;
					carryover = false;
					idx++;
				}

				auto& primitveAABB = aabbs[idx];
				auto center = 0.5f * (primitveAABB.min + primitveAABB.max);
				auto value = center[bestAxis];

				if (value >= bestSplit && leftIncrement) {
					leftIncrement = false;
					carryoverData = data[idx];
					carryoverAABB = aabbs[idx];
					continue;
				}
				else if (value < bestSplit && !leftIncrement) {
					leftIncrement = true;
					std::swap(data[idx], carryoverData);
					std::swap(aabbs[idx], carryoverAABB);
					carryover = true;
				}
				else {
					idx++;
				}
			}

			if (splitIdx > 0) {
				leftChild = nodeCount++;
				nodes.push_back(new BVHNode<T>());
				nodes[leftChild]->BuildSAH(nodes, offset, splitIdx,
					aabbs, data, nodeCount, depth + 1, true);
			}

			if (splitIdx < count) {
				rightChild = nodeCount++;
				nodes.push_back(new BVHNode<T>());
				nodes[rightChild]->BuildSAH(nodes, offset + splitIdx, count - splitIdx,
					aabbs, data, nodeCount, depth + 1, false);
			}

		}

		template <class T>
		void BVHNode<T>::GetIntersection(Ray ray, std::vector<BVHNode<T>>& nodes,
			std::vector<AABB>& aabbs, std::vector<T>& data,
			std::vector<T>& intersections) {

			auto max = std::numeric_limits<float>::max();

			auto leftIntersect = false;
			auto rightIntersect = false;

			auto leftT = max;
			auto rightT = max;

			if (leftChild > 0) {
				leftIntersect = ray.Intersects(nodes[leftChild].aabb,
					0.0f, max, leftT);
			}

			if (rightChild > 0) {
				rightIntersect = ray.Intersects(nodes[rightChild].aabb,
					0.0f, max, rightT);
			}

			for (size_t i = dataOffset; i < dataOffset + dataCount; i++) {
				if (ray.Intersects(aabbs[i], 0.0f, max))
					intersections.push_back(data[i]);
			}

			if (leftT < rightT) {
				if (leftIntersect)
					nodes[leftChild].GetIntersection(ray, nodes, aabbs, data, intersections);
				if(rightIntersect)
					nodes[rightChild].GetIntersection(ray, nodes, aabbs, data, intersections);

			}
			else {
				if (rightIntersect)
					nodes[rightChild].GetIntersection(ray, nodes, aabbs, data, intersections);
				if (leftIntersect)
					nodes[leftChild].GetIntersection(ray, nodes, aabbs, data, intersections);
			}

		}

	}

}

#endif