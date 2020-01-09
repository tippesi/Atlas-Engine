#include "PerformanceCounter.h"

namespace Atlas {

	namespace Tools {

		PerformanceCounter::PerformanceCounter() {

			Reset();

		}

		void PerformanceCounter::Reset() {

			stamps.clear();
			stamps.push_back(Stamp());

		}

		PerformanceCounter::TimeStamp PerformanceCounter::Stamp() {

			TimeStamp stamp;

			stamp.stamp = (double)SDL_GetPerformanceCounter();

			if (stamps.size()) {
				auto resetStamp = stamps[0];
				stamp.delta = (stamp.stamp - resetStamp.stamp) * 1000.0 / (double)
					SDL_GetPerformanceFrequency();
			}

			return stamp;

		}

		PerformanceCounter::TimeStamp PerformanceCounter::Stamp(TimeStamp inStamp) {

			TimeStamp stamp;

			stamp.stamp = (double)SDL_GetPerformanceCounter();

			stamp.delta = (stamp.stamp - inStamp.stamp) * 1000.0 / (double)
				SDL_GetPerformanceFrequency();

			return stamp;

		}

		PerformanceCounter::TimeStamp PerformanceCounter::StepStamp() {

			TimeStamp stamp;

			stamp.stamp = (double)SDL_GetPerformanceCounter();

			if (stamps.size()) {
				auto prevStamp = stamps[stamps.size() - 1];
				stamp.delta = (stamp.stamp - prevStamp.stamp) * 1000.0 / (double)
					SDL_GetPerformanceFrequency();
			}

			return stamp;

		}

	}

}