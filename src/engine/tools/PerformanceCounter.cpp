#include "PerformanceCounter.h"

#include <SDL3/SDL.h>

namespace Atlas {

    namespace Tools {

        PerformanceCounter::PerformanceCounter() {

            Reset();

        }

        void PerformanceCounter::Reset() {

            stamps.clear();
            Stamp();

        }

        PerformanceCounter::TimeStamp PerformanceCounter::Stamp() {

            TimeStamp stamp;

            stamp.stamp = (double)SDL_GetTicksNS();

            if (stamps.size()) {
                auto resetStamp = stamps[0];
                stamp.delta = (stamp.stamp - resetStamp.stamp) / (double)SDL_NS_PER_MS;
            }

            stamps.push_back(stamp);

            return stamp;

        }

        PerformanceCounter::TimeStamp PerformanceCounter::Stamp(TimeStamp inStamp) {

            TimeStamp stamp;

            stamp.stamp = (double)SDL_GetTicksNS();

            stamp.delta = (stamp.stamp - inStamp.stamp) / (double)SDL_NS_PER_MS;

            stamps.push_back(stamp);

            return stamp;

        }

        PerformanceCounter::TimeStamp PerformanceCounter::StepStamp() {

            TimeStamp stamp;

            stamp.stamp = (double)SDL_GetTicksNS();

            if (stamps.size()) {
                auto prevStamp = stamps[stamps.size() - 1];
                stamp.delta = (stamp.stamp - prevStamp.stamp) / (double)SDL_NS_PER_MS;
            }

            stamps.push_back(stamp);

            return stamp;

        }

    }

}