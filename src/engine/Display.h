#pragma once

#include "System.h"

#include <vector>

namespace Atlas {

    /**
     * See https://wiki.libsdl.org/SDL3/SDL_PixelFormat
     */
    struct DisplayPixelFormat {
        uint32_t type = 0;
        uint32_t order = 0;
        uint32_t layout = 0;
        uint32_t bitCount = 0;
        uint32_t byteCount = 0;
    };

    struct DisplayMode {
        DisplayPixelFormat format;

        int32_t width = 0;
        int32_t height = 0;
        float refreshRate = 0.0f;
        float pixelDensity = 1.0f;

        uint32_t displayID = 0;
        uint32_t rawFormat = 0;
    };

    class Display {

    public:
        Display() = default;

        explicit Display(uint32_t displayID);

        std::vector<DisplayMode> GetModes() const;

        uint32_t displayID = 0;

    private:
        std::vector<DisplayMode> modes;

    };

}