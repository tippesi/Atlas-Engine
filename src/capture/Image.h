#pragma once

#include "Common.h"

#include "common/Image.h"

namespace Atlas::Capture {

    // Captures are stored in engine images with 4 channels (RGBA8)
    using Image = Common::Image<uint8_t>;

    Ref<Image> CreateImage(int32_t width, int32_t height, u8vec4 color = u8vec4(0, 0, 0, 255));

    // Saved with the engine image loader
    bool WritePng(const Ref<Image>& image, const fs::path& path);

    // Returns the input if it fits maxWidth, otherwise a copy resized with the engine image resampling
    Ref<Image> Downscale(const Ref<Image>& image, int32_t maxWidth);

    void Blit(Image& target, const Image& source, int32_t x, int32_t y);

    void FillRect(Image& image, int32_t x, int32_t y, int32_t width, int32_t height, u8vec4 color);

    // Draws byte-wise text with font/roboto.ttf from the asset directory and returns its width
    // The engine font stores glyph textures on the GPU; CPU bitmaps are rasterized here with stb_truetype
    int32_t DrawText(Image& image, int32_t x, int32_t y, const std::string& text, float pixelHeight, u8vec4 color = u8vec4(255));

    // Grid of labeled images with an optional title
    Ref<Image> ComposeSheet(const std::vector<Ref<Image>>& images, const std::vector<std::string>& labels,
        int32_t columns, const std::string& title, int32_t cellWidth = 640);

}
