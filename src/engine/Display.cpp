#include "Display.h"
#include "Log.h"

#include <string>

#include <SDL3/SDL.h>

namespace Atlas {

    Display::Display(uint32_t displayID) : displayID(displayID) {

        int32_t displayModeCount = 0;
        auto displayModes = SDL_GetFullscreenDisplayModes(displayID, &displayModeCount);
        if (!displayModes) {
            auto error = SDL_GetError();
            Log::Warning("Getting display modes failed: " + std::string(error));
            return;
        }

        for (int32_t i = 0; i < displayModeCount; ++i) {
            const auto& mode = *displayModes[i];

            DisplayMode displayMode {
                .format = {
                    .type = uint32_t(SDL_PIXELTYPE(mode.format)),
                    .order = uint32_t(SDL_PIXELORDER(mode.format)),
                    .layout = uint32_t(SDL_PIXELLAYOUT(mode.format)),
                    .bitCount = uint32_t(SDL_BITSPERPIXEL(mode.format)),
                    .byteCount = uint32_t(SDL_BYTESPERPIXEL(mode.format))
                },
                .width = mode.w,
                .height = mode.h,
                .refreshRate = mode.refresh_rate,
                .pixelDensity = mode.pixel_density,
                .displayID = mode.displayID,
                .rawFormat = mode.format
            };

            modes.push_back(displayMode);
        }

        SDL_free(displayModes);

    }

    std::vector<DisplayMode> Display::GetModes() const {

        return modes;

    }

}