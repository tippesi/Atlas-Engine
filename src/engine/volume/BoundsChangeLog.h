#pragma once

#include "../System.h"
#include "AABB.h"

#include <deque>
#include <mutex>
#include <limits>

namespace Atlas {

    namespace Volume {

        /**
         * Bounds of changes which several readers pick up, e.g. navigation meshes rebuilding what changed. Each reader
         * keeps the count it has read up to. A reader which fell behind further than the log reaches gets bounds
         * covering everything.
         */
        class BoundsChangeLog {

        public:
            void Add(const AABB& aabb) {

                std::scoped_lock lock(mutex);
                changes.push_back(aabb);
                if (changes.size() > maxChanges)
                    changes.pop_front();
                count++;

            }

            std::vector<AABB> GetSince(uint64_t& readCount) const {

                std::scoped_lock lock(mutex);
                std::vector<AABB> result;
                auto missed = count - readCount;
                if (missed > changes.size())
                    result.emplace_back(vec3(-std::numeric_limits<float>::max()), vec3(std::numeric_limits<float>::max()));
                else
                    result.assign(changes.end() - missed, changes.end());
                readCount = count;
                return result;

            }

        private:
            static constexpr size_t maxChanges = 4096;

            mutable std::mutex mutex;
            std::deque<AABB> changes;
            uint64_t count = 0;

        };

    }

}
