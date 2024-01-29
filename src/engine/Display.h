#pragma once

#include "System.h"

#include <vector>

namespace Atlas {

    /**
     * See https://wiki.libsdl.org/SDL2/SDL_PixelFormatEnum
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
        int32_t refreshRate = 0;

        void* driverData = nullptr;
        uint32_t rawFormat = 0;
    };

    class Display {

    public:
        Display() = default;

        explicit Display(int32_t displayIndex);

        std::vector<DisplayMode> GetModes() const;

        int32_t displayIndex = 0;

    private:
        std::vector<DisplayMode> modes;

    };

}