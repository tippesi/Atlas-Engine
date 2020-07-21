#include "Clock.h"

#include <numeric>

namespace Atlas {

	std::mutex Clock::mutex;

	double Clock::timeStamp = 0.0;
	float Clock::deltaTime = 0.0f;

	std::vector<float> Clock::deltas;
	size_t Clock::frameCount = 0;
	size_t Clock::totalFrames = 0;

	float Clock::Get() {

		return (float)SDL_GetTicks() / 1000.0f;

	}

	float Clock::GetDelta() {

		std::lock_guard<std::mutex> lock(mutex);

		return deltaTime;

	}

	void Clock::SetAverageWindow(int32_t frames) {

		if (frames <= 0)
			return;

		std::lock_guard<std::mutex> lock(mutex);

		deltas.resize(frames);
		
		for (size_t i = 0; i < deltas.size(); i++)
			deltas[i] = 0.0f;

	}

	std::vector<float> Clock::GetAverageWindow() {

		std::lock_guard<std::mutex> lock(mutex);

		return deltas;

	}

	float Clock::GetAverage() {

		std::lock_guard<std::mutex> lock(mutex);

		auto average = std::accumulate(deltas.begin(), deltas.end(), 0.0f);

		auto size = deltas.size() < totalFrames ?
			(float)deltas.size() : (float)totalFrames;

		return average / size;

	}

	void Clock::Update() {

		std::lock_guard<std::mutex> lock(mutex);

		if (!deltas.size())
			deltas.resize(300);

		auto time = (double)SDL_GetPerformanceCounter();
		deltaTime = (float)((time - timeStamp) / (double)
			SDL_GetPerformanceFrequency());

		deltas[frameCount] = deltaTime;

		timeStamp = time;

		frameCount = (frameCount + 1) % deltas.size();
		totalFrames++;

	}

}