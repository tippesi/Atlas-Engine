#include "Clock.h"

#include <numeric>
#include <algorithm>

#include <SDL.h>

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

        std::scoped_lock<std::mutex> lock(mutex);

        return deltaTime;

    }

    void Clock::SetAverageWindow(int32_t frames) {

        if (frames <= 0)
            return;

        std::scoped_lock<std::mutex> lock(mutex);

        deltas.resize(frames);
        std::fill(deltas.begin(), deltas.end(), 0.0f);

    }

    std::vector<float> Clock::GetAverageWindow() {

        std::scoped_lock<std::mutex> lock(mutex);

        return deltas;

    }

    float Clock::GetAverage() {

        std::scoped_lock<std::mutex> lock(mutex);

        auto size = std::min(deltas.size(), totalFrames);
        auto average = std::accumulate(deltas.begin(), deltas.end() - 
            (deltas.size() - size), 0.0f);

        return average / size;

    }

    void Clock::ResetAverage() {

        std::scoped_lock<std::mutex> lock(mutex);

        std::fill(deltas.begin(), deltas.end(), 0.0f);
        totalFrames = 0;

    }

    void Clock::Update() {

        std::scoped_lock<std::mutex> lock(mutex);

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