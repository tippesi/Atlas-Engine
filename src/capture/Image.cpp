#include "Image.h"

#include "loader/ImageLoader.h"

#include <stb_truetype.h>

#include <algorithm>
#include <fstream>
#include <mutex>

namespace Atlas::Capture {

    struct FontData {

        std::vector<uint8_t> data;
        stbtt_fontinfo info;
        bool valid = false;

    };

    static FontData& GetFont() {

        static FontData font;
        static std::once_flag flag;

        std::call_once(flag, [&]() {

            std::ifstream stream(GetAssetDirectory() / "font" / "roboto.ttf", std::ios::binary);
            if (!stream.is_open())
                return;

            font.data = std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
            font.valid = stbtt_InitFont(&font.info, font.data.data(), 0) != 0;

        });

        return font;

    }

    Ref<Image> CreateImage(int32_t width, int32_t height, u8vec4 color) {

        auto image = CreateRef<Image>(width, height, 4);
        image->FillRect(0, 0, width, height, vec4(color));

        return image;

    }

    bool WritePng(const Ref<Image>& image, const fs::path& path) {

        image->fileFormat = Common::ImageFormat::PNG;
        auto mutableImage = image;
        Loader::ImageLoader::SaveImage(mutableImage, path.string());

        return fs::exists(path);

    }

    bool WriteImage(const Ref<Image>& image, const fs::path& path) {

        auto extension = path.extension().string();
        if (extension != ".jpg" && extension != ".jpeg")
            return WritePng(image, path);

        image->fileFormat = Common::ImageFormat::JPG;
        auto mutableImage = image;
        Loader::ImageLoader::SaveImage(mutableImage, path.string());

        return fs::exists(path);

    }

    Ref<Image> LoadImageFile(const fs::path& path) {

        std::error_code error;
        if (!fs::exists(path, error))
            return nullptr;
        auto image = Loader::ImageLoader::LoadImage<uint8_t>(path.string(), false, 4);
        return image && image->HasData() ? image : nullptr;

    }

    Ref<Image> CropImage(const Ref<Image>& image, ivec4 rectangle) {

        auto x0 = std::clamp(rectangle.x, 0, image->width - 1), y0 = std::clamp(rectangle.y, 0, image->height - 1);
        auto x1 = std::clamp(rectangle.x + rectangle.z, x0 + 1, image->width), y1 = std::clamp(rectangle.y + rectangle.w, y0 + 1, image->height);

        auto crop = CreateRef<Image>(x1 - x0, y1 - y0, 4);
        const auto& source = image->GetData();
        auto& target = crop->GetData();
        for (int32_t y = y0; y < y1; y++)
            std::copy_n(source.begin() + (size_t(y) * size_t(image->width) + size_t(x0)) * 4, size_t(x1 - x0) * 4,
                target.begin() + size_t(y - y0) * size_t(x1 - x0) * 4);
        return crop;

    }

    ImageDifference CompareImages(const Image& a, const Image& b, int32_t threshold, Ref<Image>* heatMap) {

        ImageDifference difference;
        if (a.width != b.width || a.height != b.height) {
            difference.sizeMismatch = true;
            return difference;
        }

        const auto& dataA = a.GetData();
        const auto& dataB = b.GetData();
        if (heatMap)
            *heatMap = CreateRef<Image>(a.width, a.height, 4);

        double sum = 0.0;
        size_t differing = 0;
        auto pixels = size_t(a.width) * size_t(a.height);
        for (size_t i = 0; i < pixels; i++) {
            int32_t pixelMax = 0;
            for (size_t c = 0; c < 3; c++) {
                auto delta = std::abs(int32_t(dataA[i * 4 + c]) - int32_t(dataB[i * 4 + c]));
                sum += double(delta);
                pixelMax = std::max(pixelMax, delta);
            }
            difference.max = std::max(difference.max, float(pixelMax));
            if (pixelMax > threshold)
                differing++;

            if (heatMap) {
                auto gray = uint8_t((int32_t(dataA[i * 4]) + int32_t(dataA[i * 4 + 1]) + int32_t(dataA[i * 4 + 2])) / 6);
                auto& heat = (*heatMap)->GetData();
                auto red = pixelMax > threshold ? uint8_t(std::min(128 + pixelMax * 2, 255)) : gray;
                heat[i * 4] = red;
                heat[i * 4 + 1] = pixelMax > threshold ? uint8_t(0) : gray;
                heat[i * 4 + 2] = pixelMax > threshold ? uint8_t(0) : gray;
                heat[i * 4 + 3] = 255;
            }
        }

        difference.mean = float(sum / double(pixels * 3));
        difference.percent = 100.0f * float(differing) / float(pixels);
        return difference;

    }

    Ref<Image> Downscale(const Ref<Image>& image, int32_t maxWidth) {

        if (image->width <= maxWidth)
            return image;

        auto scale = float(maxWidth) / float(image->width);
        auto copy = CreateRef<Image>(image->width, image->height, image->channels);
        copy->SetData(image->GetData());
        copy->Resize(maxWidth, std::max(int32_t(float(image->height) * scale), 1));

        return copy;

    }

    int32_t DrawText(Image& image, int32_t x, int32_t y, const std::string& text, float pixelHeight, u8vec4 color) {

        auto& font = GetFont();
        if (!font.valid)
            return 0;

        auto scale = stbtt_ScaleForPixelHeight(&font.info, pixelHeight);

        int32_t ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font.info, &ascent, &descent, &lineGap);
        auto baseline = y + int32_t(float(ascent) * scale);

        float cursor = float(x);
        for (size_t i = 0; i < text.size(); i++) {
            auto codepoint = int32_t(uint8_t(text[i]));

            int32_t advance, leftBearing;
            stbtt_GetCodepointHMetrics(&font.info, codepoint, &advance, &leftBearing);

            int32_t x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&font.info, codepoint, scale, scale, &x0, &y0, &x1, &y1);

            auto glyphWidth = x1 - x0, glyphHeight = y1 - y0;
            if (glyphWidth > 0 && glyphHeight > 0) {
                std::vector<uint8_t> bitmap(size_t(glyphWidth) * size_t(glyphHeight));
                stbtt_MakeCodepointBitmap(&font.info, bitmap.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, codepoint);

                for (int32_t gy = 0; gy < glyphHeight; gy++) {
                    for (int32_t gx = 0; gx < glyphWidth; gx++) {
                        auto alpha = float(bitmap[size_t(gy) * size_t(glyphWidth) + size_t(gx)]) / 255.0f;
                        image.BlendPixel(int32_t(cursor) + x0 + gx, baseline + y0 + gy, vec4(color), alpha);
                    }
                }
            }

            cursor += float(advance) * scale;
            if (i + 1 < text.size())
                cursor += scale * float(stbtt_GetCodepointKernAdvance(&font.info, codepoint, int32_t(uint8_t(text[i + 1]))));
        }

        return int32_t(cursor) - x;

    }

    Ref<Image> ComposeSheet(const std::vector<Ref<Image>>& images, const std::vector<std::string>& labels,
        int32_t columns, const std::string& title, int32_t cellWidth) {

        if (images.empty())
            return CreateImage(1, 1);

        const int32_t labelHeight = 26;
        const int32_t titleHeight = title.empty() ? 0 : 34;
        const int32_t spacing = 4;

        if (columns <= 0)
            columns = int32_t(std::ceil(std::sqrt(float(images.size()))));
        columns = std::min(columns, int32_t(images.size()));
        auto rows = (int32_t(images.size()) + columns - 1) / columns;

        std::vector<Ref<Image>> cells;
        int32_t cellHeight = 0;
        for (const auto& image : images) {
            cells.push_back(Downscale(image, cellWidth));
            cellHeight = std::max(cellHeight, cells.back()->height);
        }
        cellWidth = std::min(cellWidth, cells.front()->width);

        auto width = columns * (cellWidth + spacing) + spacing;
        auto height = titleHeight + rows * (cellHeight + labelHeight + spacing) + spacing;
        auto sheet = CreateImage(width, height, u8vec4(24, 24, 24, 255));

        if (!title.empty())
            DrawText(*sheet, spacing + 4, 6, title, 22.0f);

        for (size_t i = 0; i < cells.size(); i++) {
            auto column = int32_t(i) % columns, row = int32_t(i) / columns;
            auto x = spacing + column * (cellWidth + spacing);
            auto y = titleHeight + spacing + row * (cellHeight + labelHeight + spacing);

            sheet->FillRect(x, y, cellWidth, labelHeight, vec4(48, 48, 48, 255));
            if (i < labels.size())
                DrawText(*sheet, x + 6, y + 4, labels[i], 17.0f);

            sheet->Blit(*cells[i], x, y + labelHeight);
        }

        return sheet;

    }

}
