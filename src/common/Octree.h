#ifndef AE_OCTREE_H
#define AE_OCTREE_H

#include "../System.h"
#include "AABB.h"

#include <vector>
#include <unordered_set>
#include <algorithm>

namespace Atlas {

    namespace Common {

        template <class T> class Octree {

        public:
            Octree() {}

            Octree(AABB aabb, int32_t capacity, int32_t depth);

            bool Insert(T data, AABB aabb);

            void Remove(T data, AABB aabb);

            void QueryAABB(std::unordered_set<T>& data, AABB aabb);

            bool IsSubdivided();

            std::vector<Octree<T>> GetChildren();

			void Clear();

            std::vector<T> octreeData;

            AABB aabb;

			int32_t removeCount = 0;

        private:
            void Subdivide();

            std::vector<Octree<T>> children;

            int32_t capacity;
            int32_t depth;

        };

        template <class T>
        Octree<T>::Octree(AABB aabb, int32_t capacity, int32_t depth) :
                aabb(aabb), capacity(capacity), depth(depth) {



        }

        template <class T>
        bool Octree<T>::Insert(T data, AABB aabb) {

            if (!this->aabb.Intersects(aabb)) {
                return false;
            }

            if (octreeData.size() < capacity) {
                octreeData.push_back(data);
                return true;
            }

            bool successful = false;

            if (depth > 0) {

                if (children.size() == 0)
                    Subdivide();

                for (auto &child : children) {
                    successful |= child.Insert(data, aabb);
                }

            }

            return successful;

        }

        template <class T>
        void Octree<T>::Remove(T data, AABB aabb) {

            if (!this->aabb.Intersects(aabb))
                return;

			removeCount++;

			auto item = std::find(octreeData.begin(), octreeData.end(), data);

			if (item != octreeData.end()) {
				octreeData.erase(item);
				return;
			}

            if (children.size() == 0)
                return;

            bool removeChildren = true;

            for (auto& child : children) {
                child.Remove(data, aabb);
                removeChildren = removeChildren && child.octreeData.size() == 0 && !child.IsSubdivided();
            }

            if (removeChildren)
                children.resize(0);

        }

        template <class T>
        void Octree<T>::QueryAABB(std::unordered_set<T>& data, AABB aabb) {

			if (!this->aabb.Intersects(aabb))
				return;

			for (auto& queryData : octreeData)
			    data.insert(queryData);

			for (auto& child : children) {
				child.QueryAABB(data, aabb);
			}

        }

        template <class T>
        bool Octree<T>::IsSubdivided() {

            return children.size() != 0;

        }

        template <class T>
        std::vector<Octree<T>> Octree<T>::GetChildren() {

            return children;

        }

		template <class T>
		void Octree<T>::Clear() {

			children.clear();
			octreeData.clear();

		}

        template <class T>
        void Octree<T>::Subdivide() {

            vec3 min = aabb.min;
            vec3 max = aabb.max;
            vec3 center = aabb.min + 0.5f * (aabb.max - aabb.min);

            AABB aabbs[] = {
                    AABB(vec3(min.x, min.y, min.z), vec3(center.x, center.y, center.z)),
                    AABB(vec3(center.x, min.y, min.z), vec3(max.x, center.y, center.z)),
                    AABB(vec3(min.x, min.y, center.z), vec3(center.x, center.y, max.z)),
                    AABB(vec3(center.x, min.y, center.z), vec3(max.x, center.y, max.z)),
                    AABB(vec3(min.x, center.y, min.z), vec3(center.x, max.y, center.z)),
                    AABB(vec3(center.x, center.y, min.z), vec3(max.x, max.y, center.z)),
                    AABB(vec3(min.x, center.y, center.z), vec3(center.x, max.y, max.z)),
                    AABB(vec3(center.x, center.y, center.z), vec3(max.x, max.y, max.z))};

            children.resize(8);

            for (uint8_t i = 0; i < 8; i++) {
                children[i] = Octree<T>(aabbs[i], capacity, depth - 1);
            }

        }

    }

}

#endif