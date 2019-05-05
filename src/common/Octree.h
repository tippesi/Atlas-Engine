#ifndef AE_OCTREE_H
#define AE_OCTREE_H

#include "../System.h"
#include "AABB.h"

#include <vector>
#include <unordered_set>
#include <algorithm>

namespace Atlas {

    namespace Common {

        /**
         * Based on https://anteru.net/blog/2008/loose-octrees/
         * @tparam T
         */
        template <class T> class Octree {

        public:
            Octree() {}

            Octree(AABB aabb, int32_t depth, float relaxFactor = 2.0f);

            bool Insert(T data, AABB aabb);

            void Remove(T data, AABB aabb);

            void QueryAABB(std::vector<T>& data, AABB aabb);

			void GetData(std::vector<T>& data);

            bool IsSubdivided();

            std::vector<Octree<T>> GetChildren();

			void Clear();

            std::vector<T> octreeData;

            AABB aabb;

        private:
            void Subdivide();

            bool InsertInternal(T data, AABB aabb, vec3 center);

            bool RemoveInternal(T data, AABB aabb, vec3 center);

            std::vector<Octree<T>> children;

            int32_t depth;

            float relaxFactor;

        };

        template <class T>
        Octree<T>::Octree(AABB aabb, int32_t depth, float relaxFactor) :
                aabb(aabb), depth(depth), relaxFactor(relaxFactor) {



        }

        template <class T>
        bool Octree<T>::Insert(T data, AABB aabb) {

            auto center = aabb.min + 0.5f * (aabb.max - aabb.min);

            return InsertInternal(data, aabb, center);

        }

        template <class T>
        void Octree<T>::Remove(T data, AABB aabb) {

            auto center = aabb.min + 0.5f * (aabb.max - aabb.min);

            RemoveInternal(data, aabb, center);

        }

        template <class T>
        void Octree<T>::QueryAABB(std::vector<T>& data, AABB aabb) {

			auto scaled = this->aabb.Scale(relaxFactor);

			if (!scaled.Intersects(aabb))
				return;

			// Check if aabb encloses this octree node
			// In that case we can add all data and children data
			if (aabb.IsInside(scaled)) {
				GetData(data);
				return;
			}

			for (auto& ele : octreeData)
			    data.push_back(ele);

			for (auto& child : children)
				child.QueryAABB(data, aabb);

        }

		template <class T>
		void Octree<T>::GetData(std::vector<T>& data) {

			for (auto& ele : octreeData)
				data.push_back(ele);

			for (auto& child : children)
				child.GetData(data);

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
                children[i] = Octree<T>(aabbs[i], depth - 1, relaxFactor);
            }

        }

        template <class T>
        bool Octree<T>::InsertInternal(T data, AABB aabb, vec3 center) {

            if (!this->aabb.IsInside(center)) {
                return false;
            }

            if (!this->aabb.Scale(relaxFactor).IsInside(aabb)) {
                return false;
            }

            bool successful = false;

            if (depth > 0) {

                if (children.size() == 0)
                    Subdivide();

                for (auto &child : children) {
					if (successful)
						break;
                    successful |= child.InsertInternal(data, aabb, center);
                }

            }

			if (!successful)
				octreeData.push_back(data);

            return true;

        }

        template <class T>
        bool Octree<T>::RemoveInternal(T data, AABB aabb, vec3 center) {

            if (!this->aabb.IsInside(center)) {
                return false;
            }

			if (!this->aabb.Scale(relaxFactor).IsInside(aabb)) {
				return false;
			}

            auto item = std::find(octreeData.begin(), octreeData.end(), data);

            if (item != octreeData.end()) {
                octreeData.erase(item);
                return true;
            }

            if (children.size() == 0)
                return false;

            bool removeChildren = true;
			bool successful = false;

            for (auto& child : children) {
				if (!successful)
					successful |= child.RemoveInternal(data, aabb, center);
                removeChildren = removeChildren && child.octreeData.size() == 0 && !child.IsSubdivided();
            }

            if (removeChildren)
                children.resize(0);

			return successful;

        }

    }

}

#endif