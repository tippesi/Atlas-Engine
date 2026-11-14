#pragma once

#include "Common.h"

#include "common/Image.h"

namespace Atlas::Capture {

    // Captures are stored in engine images with 4 channels (RGBA8)
    using Image = Common::Image<uint8_t>;

    Ref<Image> CreateImage(int32_t width, int32_t height, u8vec4 color = u8vec4(0, 0, 0, 255));

    // Saved with the engine image loader
    bool WritePng(const Ref<Image>& image, const fs::path& path);

    // A PNG or a JPG by the extension of the path
    bool WriteImage(const Ref<Image>& image, const fs::path& path);

    // nullptr if the file can't be read
    Ref<Image> LoadImageFile(const fs::path& path);

    // The part of the image inside the rectangle (x, y, width, height), clamped to the image
    Ref<Image> CropImage(const Ref<Image>& image, ivec4 rectangle);

    struct ImageDifference {

        // Of the color channels on a scale of 0 to 255
        float mean = 0.0f;
        float max = 0.0f;
        // Pixels with a channel that differs by more than the threshold
        float percent = 0.0f;
        bool sizeMismatch = false;

    };

    // Optionally writes a heat map: the first image in gray, differences above the threshold in red
    ImageDifference CompareImages(const Image& a, const Image& b, int32_t threshold, Ref<Image>* heatMap = nullptr);

    // Returns the input if it fits maxWidth, otherwise a copy resized with the engine image resampling
    Ref<Image> Downscale(const Ref<Image>& image, int32_t maxWidth);

    // Draws byte-wise text with font/roboto.ttf from the asset directory and returns its width
    // The engine font stores glyph textures on the GPU; CPU bitmaps are rasterized here with stb_truetype
    int32_t DrawText(Image& image, int32_t x, int32_t y, const std::string& text, float pixelHeight, u8vec4 color = u8vec4(255));

    // Grid of labeled images with an optional title
    Ref<Image> ComposeSheet(const std::vector<Ref<Image>>& images, const std::vector<std::string>& labels,
        int32_t columns, const std::string& title, int32_t cellWidth = 640);

}
