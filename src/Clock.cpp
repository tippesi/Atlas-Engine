#include "Clock.h"

#include <SDL/include/SDL.h>

namespace Atlas {

	std::mutex Clock::mutex;

	float Clock::timeStamp = 0.0f;
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

	float Clock::GetAverage() {

		std::lock_guard<std::mutex> lock(mutex);

		float average = 0.0f;

		for (auto delta : deltas)
			average += delta;

		auto size = deltas.size() < totalFrames ?
			(float)deltas.size() : (float)totalFrames;

		return average / size;

	}

	void Clock::Update() {

		std::lock_guard<std::mutex> lock(mutex);

		if (!deltas.size())
			deltas.resize(300);

		auto time = Get();
		deltaTime = time - timeStamp;

		deltas[frameCount] = deltaTime;

		timeStamp = time;

		frameCount = (frameCount + 1) % deltas.size();
		totalFrames++;

	}

}