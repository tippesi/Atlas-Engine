#ifndef AE_TIME_H
#define AE_TIME_H

#include "System.h"

#include <mutex>
#include <vector>

namespace Atlas {

	class Clock {

	public:
		/**
		 * Returns the time in seconds.
		 * @return The time as a float in seconds.
		 */
		static float Get();

		/**
		 * Returns the delta time in seconds.
		 * @return The delta time as a float in seconds.
		 */
		static float GetDelta();

		/**
		 * Sets the number of frames taken into account for the average calculation
		 * @param frames The number of frames
		 */
		static void SetAverageWindow(int32_t frames);

		/**
		 * Returns the average window with the last delta values
		 * @return The average window with the last delta values.
		 */
		static std::vector<float> GetAverageWindow();

		/**
		 * Returns the average time in seconds.
		 * @return The average time in seconds
		 */
		static float GetAverage();

		/**
		 * Resets the average.
		 */
		static void ResetAverage();

		/**
	     * Updates the time. Is called by the engine.
		 */
		static void Update();

	private:
		static std::mutex mutex;

		static double timeStamp;
		static float deltaTime;

		static std::vector<float> deltas;
		static size_t frameCount;
		static size_t totalFrames;

	};

}

#endif