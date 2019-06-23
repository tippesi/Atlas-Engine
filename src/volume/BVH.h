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
			BVHNode() {}

			void Init(std::vector<BVHNode<T>*>& nodes, int32_t sortAxis, size_t offset, size_t count,
				std::vector<std::pair<AABB, T>>& data, int32_t& nodeCount, int32_t depth, bool isLeftChild);

			void GetIntersection(Ray ray, std::vector<BVHNode<T>>& nodes,
				std::vector<AABB>& aabbs, std::vector<T>& data,
				std::vector<T>& intersections);

			AABB aabb;

			int32_t leftChild = -1;
			int32_t rightChild = -1;

			int32_t dataOffset = 0;
			int32_t dataCount = 0;

			int32_t depth = 0;
			bool isLeftChild = false;

		};

		template <class T> class BVH {

		public:
			BVH() {}

			BVH(std::vector<AABB> aabbs, std::vector<T> data);

			std::vector<T> GetIntersection(Ray ray);

			std::vector<BVHNode<T>> GetTree();

			std::vector<AABB> aabbs;
			std::vector<T> data;

		private:
			std::vector<BVHNode<T>> nodes;

		};

		template <class T>
		BVH<T>::BVH(std::vector<AABB> aabbs, std::vector<T> data) {

			if (aabbs.size() != data.size())
				return;

			std::vector<std::pair<AABB, T>> nodeData;

			for (size_t i = 0; i < aabbs.size(); i++) {
				nodeData.push_back(std::pair<AABB, T>(
					aabbs[i], data[i]
					));
			}

			int32_t nodeCount = 1;

			// We need to use pointers first and copy memory later
			// because when the vector resizes we still work on elements
			// of that vector which leads to errors.
			std::vector<BVHNode<T>*> nodesPointer;

			nodesPointer.push_back(new BVHNode<T>());
			nodesPointer[0]->Init(nodesPointer, 0, 0, nodeData.size(),
				nodeData, nodeCount, 0, true);

			// Copy nodes
			for (auto node : nodesPointer) {
				nodes.push_back(*node);
				delete node;
			}

			// Copy data
			this->aabbs.resize(aabbs.size());
			this->data.resize(data.size());

			for (size_t i = 0; i < nodeData.size(); i++) {
				this->aabbs[i] = nodeData[i].first;
				this->data[i] = nodeData[i].second;
			}

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
		std::vector<BVHNode<T>> BVH<T>::GetTree() {

			return nodes;

		}

		template <class T>
		void BVHNode<T>::Init(std::vector<BVHNode<T>*>& nodes, int32_t sortAxis, size_t offset,
			size_t count, std::vector<std::pair<AABB, T>>& data, int32_t& nodeCount, int32_t depth, bool isLeftChild) {

			this->depth = depth;
			this->isLeftChild = isLeftChild;

			// Calculate AABB for node
			auto min = vec3(std::numeric_limits<float>::max());
			auto max = vec3(-std::numeric_limits<float>::max());

			for (size_t i = offset; i < offset + count; i++) {
				min = glm::min(min, data[i].first.min);
				max = glm::max(max, data[i].first.max);
			}

			aabb = AABB(min, max);

			// Check if data is big enough to continue splitting
			if (count > 4) {

				// Sort in place
				std::sort(data.begin() + offset, data.begin() + offset + count,
					[=](std::pair<AABB, T> data1, std::pair<AABB, T> data2) -> bool {

						auto aabb1 = data1.first;
						auto aabb2 = data2.first;

						auto center1 = 0.5f * (aabb1.max + aabb1.min);
						auto center2 = 0.5f * (aabb2.max + aabb2.min);

						return center1[sortAxis] < center2[sortAxis];

					});

				// Split by splitting the data in half
				auto split = count / 2;

				if (split > 0) {
					leftChild = nodeCount++;
					nodes.push_back(new BVHNode<T>());
					nodes[leftChild]->Init(nodes, (sortAxis + 1) % 3, 
						offset, split, data, nodeCount, depth + 1, true);
				}

				if (split < count) {
					rightChild = nodeCount++;
					nodes.push_back(new BVHNode<T>());
					nodes[rightChild]->Init(nodes, (sortAxis + 1) % 3, 
						offset + split, count - split, data, nodeCount, depth + 1, false);
				}

			}
			else {

				dataOffset = (int32_t)offset;
				dataCount = (int32_t)count;

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