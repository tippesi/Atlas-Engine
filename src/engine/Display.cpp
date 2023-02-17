#include "Display.h"
#include "Log.h"

#include <string>

#include "SDL.h"

namespace Atlas {

    Display::Display(int32_t displayIndex) : displayIndex(displayIndex) {

        auto displayModeCount = SDL_GetNumDisplayModes(displayIndex);
        if (displayModeCount < 1) {
            auto error = SDL_GetError();
            Log::Warning("Getting display modes count failed: " + std::string(error));
            return;
        }

        for (int32_t i = 0; i < displayModeCount; ++i) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(displayIndex, i, &mode) != 0) {
                auto error = SDL_GetError();
                Log::Warning("Getting display mode failed: " + std::string(error));
            }

            DisplayMode displayMode {
                .format = {
                    .type = SDL_PIXELTYPE(mode.format),
                    .order = SDL_PIXELORDER(mode.format),
                    .layout = SDL_PIXELLAYOUT(mode.format),
                    .bitCount = SDL_BITSPERPIXEL(mode.format),
                    .byteCount = SDL_BYTESPERPIXEL(mode.format)
                },
                .width = mode.w,
                .height = mode.h,
                .refreshRate = mode.refresh_rate,
                .driverData = mode.driverdata,
                .rawFormat = mode.format
            };

            modes.push_back(displayMode);
        }

    }

    std::vector<DisplayMode> Display::GetModes() const {

        return modes;

    }

}