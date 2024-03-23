#ifndef AE_OCTREE_H
#define AE_OCTREE_H

#include "../System.h"
#include "AABB.h"
#include "Ray.h"
#include "Frustum.h"

#include <vector>
#include <unordered_set>
#include <algorithm>
#include <iterator>

namespace Atlas {

    namespace Volume {

        /**
         * Based on https://anteru.net/blog/2008/loose-octrees/
         * @tparam T
         */
        template <class T> class Octree {

        public:
            Octree() = default;

            Octree(AABB aabb, int32_t depth, float relaxFactor = 2.0f);

            Octree& operator=(const Octree& that);

            bool Insert(T data, AABB aabb);

            void Remove(T data, AABB aabb);

            void QueryAABB(std::vector<T>& data, AABB aabb);

            void QueryRay(std::vector<T>& data, Ray ray);

            void QueryFrustum(std::vector<T>& data, std::vector<T>& insideData, Frustum frustum);

            void GetData(std::vector<T>& data) const;

            void Sort();

            bool IsSubdivided() const;

            std::vector<Octree<T>> GetChildren() const;

            void Clear();

            std::vector<T> octreeData;

            AABB aabb;

            size_t dataCount = 0;

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
        Octree<T>& Octree<T>::operator=(const Octree<T>& that) {

            if (this != &that) {

                depth = that.depth;
                relaxFactor = that.relaxFactor;
                aabb = that.aabb;

                octreeData = that.octreeData;

                if (that.IsSubdivided()) {

                    children.resize(8);

                    for (uint8_t i = 0; i < 8; i++) {
                        children[i] = that.children[i];
                    }

                }

            }

            return *this;

        }

        template <class T>
        bool Octree<T>::Insert(T data, AABB aabb) {

            dataCount++;
            auto center = aabb.min + 0.5f * (aabb.max - aabb.min);

            return InsertInternal(data, aabb, center);

        }

        template <class T>
        void Octree<T>::Remove(T data, AABB aabb) {

            dataCount--;
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

            std::copy(octreeData.begin(), octreeData.end(), back_inserter(data));

            for (auto& child : children)
                child.QueryAABB(data, aabb);

        }

        template <class T>
        void Octree<T>::QueryRay(std::vector<T>& data, Ray ray) {

            auto scaled = this->aabb.Scale(relaxFactor);

            if (!ray.Intersects(scaled))
                return;

            std::copy(octreeData.begin(), octreeData.end(), back_inserter(data));

            for (auto& child : children)
                child.QueryRay(data, ray);

        }

        template <class T>
        void Octree<T>::QueryFrustum(std::vector<T>& data, std::vector<T>& insideData, Frustum frustum) {

            auto scaled = this->aabb.Scale(relaxFactor);

            if (!frustum.Intersects(scaled))
                return;

            // Check if frustum encloses this octree node
            // In that case we can add all data and children data
            if (frustum.IsInside(scaled)) {
                GetData(insideData);
                return;
            }

            std::copy(octreeData.begin(), octreeData.end(), back_inserter(data));

            for (auto& child : children)
                child.QueryFrustum(data, insideData, frustum);

        }

        template <class T>
        void Octree<T>::GetData(std::vector<T>& data) const {

            std::copy(octreeData.begin(), octreeData.end(), back_inserter(data));

            for (auto& child : children)
                child.GetData(data);

        }

        template <class T>
        void Octree<T>::Sort() {

            if (octreeData.size() > 1)
                std::sort(octreeData.begin(), octreeData.end());

            for (auto& child : children)
                child.Sort();

        }

        template <class T>
        bool Octree<T>::IsSubdivided() const {

            return children.size() != 0;

        }

        template <class T>
        std::vector<Octree<T>> Octree<T>::GetChildren() const {

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