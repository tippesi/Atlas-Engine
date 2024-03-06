#pragma once

#include "../System.h"
#include "../Filter.h"

#include <stb_image_resize.h>

#include <vector>
#include <type_traits>


namespace Atlas {

    namespace Common {

        /**
         * Image file formats.
         * Not all formats support all image types.
         * For saving images the following types are supported:
         * PNG: uint8_t
         * JPG: uint8_t
         * BMP: uint8_t
         * PGM: uint8_t, uint16_t
         * HDR: float
         * For reading images the following types are supported:
         * PNG: uint8_t, uint16_t
         * JPG: uint8_t
         * BMP: uint8_t
         * PGM: uint8_t, uint16_t
         * HDR: float
         */
        enum class ImageFormat {
            PNG = 0,
            JPG,
            BMP,
            PGM,
            HDR
        };

        /**
         * An image template class.
         * @tparam T The format type of the image (on a per channel basis)
         * @note Supported formats are: uint8_t, uint16_t and float.
         */
        template<typename T>
        class Image {

            static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, float>, "Unsupported image format. Supported are uint8_t, uint16_t and float");

        public:
            /**
             * Represents a mip map level of the image.
             * @tparam S The format type of the image
             */
            template<typename S>
            struct MipLevel {

                int32_t width = 0;
                int32_t height = 0;

                std::vector<S> data;

            };

            /**
             * Default constructor.
             */
            Image() = default;

            /**
             * Constructs an image object.
             * @param width The width of the image in pixels.
             * @param height The height of the image in pixels
             * @param channels The number of channels per pixel.
             */
            Image(int32_t width, int32_t height, int32_t channels);

            /**
             * Sets the data of the image at mipmap level 0.
             * @param data The new image data.
             * @note Updating the data won't result in an update of
             * the mipmap chain. Always call GenerateMipmap() for that.
             */
            void SetData(const std::vector<T>& data);

            /**
             * Sets the data of the image at mipmap level 0 at the given position.
             * @param x The x coordinate of the pixel to be set
             * @param y The y coordinate of the pixel to be set
             * @param channel The channel which should be set
             * @param data The data to be set at position (x,y) and channel index channel.
             * @note Updating the data won't result in an update of
             * the mipmap chain. Always call GenerateMipmap() for that.
             */
            void SetData(int32_t x, int32_t y, int32_t channel, T data);

            /**
             *
             * @tparam S The type of the dat
             * @param x The x coordinate of the pixel to be set
             * @param y The y coordinate of the pixel to be set
             * @param data The data to be set at position (x,y) and channel index channel.
             * @note Updating the data won't result in an update of
             * the mipmap chain. Always call GenerateMipmap() for that.
             */
            template<class S>
            void SetData(int32_t x, int32_t y, S data);

            /**
             * Returns the image data at mipmap level 0.
             * @return The image data at mipmap level 0.
             */
            std::vector<T> GetData() const;

            /**
             * Returns the image data at mipmap level 0.
             * @return The image data at mipmap level 0.
             */
            std::vector<T>& GetData();

            /**
             * Check whether or not the image constains data.
             * @return Whether or not the image constains data.
             */
            bool HasData() const;

            /**
             * Fills the whole image with a single value
             * @param value
             */
            void Fill(T value);

            /**
             * Returns the mipmap level structure at a given level.
             * @param mipLevel The mipmap level which should be returned.
             * @return The mipmap level structure at level mipLevel.
             */
            MipLevel<T>& GetMipLevel(int32_t mipLevel) const;

            /**
             * Returns the selected channel data of the whole image.
             * @param channelOffset The offset of the channels to be copied.
             * @param channelCount The number of channels to be copied.
             * @param mipLevel The mipmap level of the data.
             * @return The channel data of the whole image.
             * @note The resulting data will contain all pixel data of
             * the channels in range [channelOffset, channelOffset + channelCount].
             * At each pixel, this data range will be copied in the to be returned data.
             */
            std::vector<T> GetChannelData(int32_t channelOffset,
                int32_t channelCount, int32_t mipLevel = 0) const;

            /**
             * Returns the selected channel data of the whole image as an image.
             * @param channelOffset The offset of the channels to be copied.
             * @param channelCount The number of channels to be copied.
             * @param mipLevel The mipmap level of the data.
             * @return The channel data of the whole image.
             * @note The resulting data will contain all pixel data of
             * the channels in range [channelOffset, channelOffset + channelCount].
             * At each pixel, this data range will be copied in the to be returned data.
             */
            Ref<Image<T>> GetChannelImage(int32_t channelOffset,
                int32_t channelCount, int32_t mipLevel = 0) const;

            /**
             * Returns the number of mipmap levels the image has.
             * @return The number of mipmap levels the image has.
             */
            int32_t GetMipmapLevelCount() const;

            /**
             * Generates the mipmap chain.
             */
            void GenerateMipmap();

            /**
             * Resizes the image.
             * @param width The new width of the image in pixels.
             * @param height The new height of the image in pixels.
             * @note Resizing the image will result in a loss of data
             * of the mipmap chain. Call GenerateMipmap() to generate it again.
             */
            void Resize(int32_t width, int32_t height);

            /**
             * Applies a filter to the whole image.
             * @param filter Then filter to be applied.
             */
            void ApplyFilter(Filter filter);

            /**
             * Converts the image data from gamma to linear color space.
             * @note Converting the image data will result in a loss of data
             * of the mipmap chain. Call GenerateMipmap() to generate it again.
             */
            void GammaToLinear();

            /**
             * Flips image data horizontally.
             * @note Flipping the image data will result in a loss of data
             * of the mipmap chain. Call GenerateMipmap() to generate it again.
             */
            void FlipHorizontally();

            /**
             * Expands the images channels
             * @param channels The amount of channels the image should expand to
             * @param fill The data used to expand the missing channels
             */
            void ExpandToChannelCount(int32_t channels, T fill);

            /**
             * Converts the data into the specified type
             * @tparam R
             * @return
             */
            template<typename R>
            std::vector<R> ConvertData();

            /**
             * Determines the maximum pixel value depending on the format type T.
             * @return The maximum pixel value depending on the format type T.
             */
            inline float MaxPixelValue() const;

            /**
             * Samples the image at a given position.
             * @param x The x coordinate in pixels.
             * @param y The y coordinate in pixels.
             * @param mipLevel The mipmap level to be sampled.
             * @return The method returns the sampled data. The return type
             * will depend on the type T of the image. In case of an integer image
             * it will be an ivec4, for a floating point image it will be a vec4.
             */
            inline decltype(auto) Sample(int32_t x, int32_t y, int32_t mipLevel = 0) const {
                auto& level = mipLevels[mipLevel];

                auto width = level.width;
                auto height = level.height;

                x = x < 0 ? 0 : x;
                x = x >= width ? width - 1 : x;

                y = y < 0 ? 0 : y;
                y = y >= height ? height - 1 : y;

                auto index = (y * width + x) * channels;

                if constexpr (std::is_integral_v<T>) {
                    glm::ivec4 result(0);

                    for (int32_t i = 0; i < channels; i++)
                        result[i] = int32_t(level.data[index + i]);

                    return result;
                }
                else {
                    glm::vec4 result(0);

                    for (int32_t i = 0; i < channels; i++)
                        result[i] = float(level.data[index + i]);

                    return result;
                }
            }

            /**
             * Samples the image at a given position.
             * @param x The x coordinate in range of [0,1].
             * @param y The y coordinate in range of [0,1].
             * @param mipLevel The mipmap level to be sampled.
             * @return The method returns the sampled data. The return type
             * will depend on the type T of the image. In case of an integer image
             * it will be an ivec4, for a floating point image it will be a vec4.
             */
            inline decltype(auto) Sample(float x, float y, int32_t mipLevel = 0) const {
                auto& level = mipLevels[mipLevel];

                auto fWidth = float(level.width);
                auto fHeight = float(level.height);

                x *= fWidth;
                y *= fHeight;

                auto ix = int32_t(glm::clamp(x, 0.0f, fWidth - 1.0f));
                auto iy = int32_t(glm::clamp(y, 0.0f, fHeight - 1.0f));

                auto index = (iy * width + ix) * channels;

                if constexpr (std::is_integral_v<T>) {
                    glm::ivec4 result(0);

                    for (int32_t i = 0; i < channels; i++)
                        result[i] = int32_t(level.data[index + i]);

                    return result;
                }
                else {
                    glm::vec4 result(0);

                    for (int32_t i = 0; i < channels; i++)
                        result[i] = float(level.data[index + i]);

                    return result;
                }
            }

            /**
             * Bilinearly samples the image at a given position.
             * @param x The x coordinate in range of [0,1].
             * @param y The y coordinate in range of [0,1].
             * @param mipLevel The mipmap level to be sampled.
             * @return The method returns the sampled data. The return type
             * will depend on the type T of the image. In case of an integer image
             * it will be an ivec4, for a floating point image it will be a vec4.
             */
            inline decltype(auto) SampleBilinear(float x, float y, int32_t mipLevel = 0) const {
                auto& level = mipLevels[mipLevel];

                auto fWidth = float(level.width);
                auto fHeight = float(level.height);

                x *= fWidth;
                y *= fHeight;

                auto ox = int32_t(x);
                auto oy = int32_t(y);

                x = x - floorf(x);
                y = y - floorf(y);

                if (channels == 1) {
                    // Fetch the four texture samples
                    auto q00 = float(Sample(ox, oy, mipLevel).r);
                    auto q10 = float(Sample(ox + 1, oy, mipLevel).r);
                    auto q01 = float(Sample(ox, oy + 1, mipLevel).r);
                    auto q11 = float(Sample(ox + 1, oy + 1, mipLevel).r);

                    // Interpolate samples horizontally
                    auto h0 = (1.0f - x) * q00 + x * q10;
                    auto h1 = (1.0f - x) * q01 + x * q11;

                    if constexpr (std::is_integral_v<T>) {
                        return glm::ivec4(int32_t((1.0f - y) * h0 + y * h1));
                    }
                    else {
                        return glm::vec4((1.0f - y) * h0 + y * h1);
                    }
                }
                else if (channels == 3) {
                    // Fetch the four texture samples
                    auto q00 = glm::vec3(Sample(ox, oy, mipLevel));
                    auto q10 = glm::vec3(Sample(ox + 1, oy, mipLevel));
                    auto q01 = glm::vec3(Sample(ox, oy + 1, mipLevel));
                    auto q11 = glm::vec3(Sample(ox + 1, oy + 1, mipLevel));

                    // Interpolate samples horizontally
                    auto h0 = (1.0f - x) * q00 + x * q10;
                    auto h1 = (1.0f - x) * q01 + x * q11;

                    if constexpr (std::is_integral_v<T>) {
                        return glm::ivec4((1.0f - y) * h0 + y * h1, 1.0f);
                    }
                    else {
                        return glm::vec4((1.0f - y) * h0 + y * h1, 1.0f);
                    }
                }
                else {
                    // Fetch the four texture samples
                    auto q00 = glm::vec4(Sample(ox, oy, mipLevel));
                    auto q10 = glm::vec4(Sample(ox + 1, oy, mipLevel));
                    auto q01 = glm::vec4(Sample(ox, oy + 1, mipLevel));
                    auto q11 = glm::vec4(Sample(ox + 1, oy + 1, mipLevel));

                    // Interpolate samples horizontally
                    auto h0 = (1.0f - x) * q00 + x * q10;
                    auto h1 = (1.0f - x) * q01 + x * q11;

                    if constexpr (std::is_integral_v<T>) {
                        return glm::ivec4((1.0f - y) * h0 + y * h1);
                    }
                    else {
                        return (1.0f - y) * h0 + y * h1;
                    }
                }
            }

            int32_t width = 0;
            int32_t height = 0;
            int32_t channels = 0;

            ImageFormat fileFormat = ImageFormat::PNG;
            std::string fileName = "";

        private:
            std::vector<MipLevel<T>> mipLevels;

        };

        template<typename T>
        Image<T>::Image(int32_t width, int32_t height, int32_t channels) :
            width(width), height(height), channels(channels) {

            Resize(width, height);

        }

        template<typename T>
        void Image<T>::SetData(const std::vector<T>& data) {

            mipLevels[0].data = data;

        }

        template<typename T>
        void Image<T>::SetData(int32_t x, int32_t y, int32_t channel, T data) {

            auto index = (y * width + x) * channels;

            mipLevels[0].data[index + channel] = data;

        }

        template<typename T>
        template<typename S>
        void Image<T>::SetData(int32_t x, int32_t y, S value) {

            AE_ASSERT(sizeof(S) / 4 == size_t(channels) && "Data can't be fitted into channels");

            if constexpr (std::is_integral_v<T> && (std::is_same_v<S, glm::ivec2>
                || std::is_same_v<S, glm::ivec3> || std::is_same_v<S, glm::ivec4>)) {
                auto min = std::min(int32_t(sizeof(S)) / 4, channels);
                auto index = (y * width + x) * channels;

                for (int32_t i = 0; i < min; i++) {
                    mipLevels[0].data[index + i] = T(value[i]);
                }
            }
            else if constexpr ((!std::is_integral_v<T> && (std::is_same_v<S, glm::vec2>
                || std::is_same_v<S, glm::vec3> || std::is_same_v<S, glm::vec4>))) {
                auto min = std::min(int32_t(sizeof(S)) / 4, channels);
                auto index = (y * width + x) * channels;

                for (int32_t i = 0; i < min; i++) {
                    mipLevels[0].data[index + i]= T(value[i]);
                }
            }
            else if constexpr (!std::is_integral_v<T> && std::is_same_v<S, float>) {
                auto min = std::min(int32_t(sizeof(S)) / 4, channels);
                auto index = (y * width + x) * channels;

                for (int32_t i = 0; i < min; i++) {
                    mipLevels[0].data[index + i]= T(value);
                }
            }
            else {
                static_assert(!sizeof(S), "Unsupported data type");
            }

        }

        template<typename T>
        std::vector<T> Image<T>::GetData() const {

            return mipLevels[0].data;

        }

        template<typename T>
        std::vector<T>& Image<T>::GetData() {

            return mipLevels[0].data;

        }

        template<typename T>
        bool Image<T>::HasData() const {

            return mipLevels.size() > 0 &&
                mipLevels[0].data.size() > 0;

        }

        template<typename T>
        void Image<T>::Fill(T value) {

            std::fill(GetData().begin(), GetData().end(), value);

        }

        template<typename T>
        typename Image<T>::template MipLevel<T>& Image<T>::GetMipLevel(int32_t mipLevel) const {

            return mipLevels[mipLevel];

        }

        template<typename T>
        std::vector<T> Image<T>::GetChannelData(int32_t channelOffset,
            int32_t channelCount, int32_t mipLevel) const {

            std::vector<T> channelData;
            auto& level = mipLevels[mipLevel];

            auto totalPixels = int32_t(level.data.size()) / channels;
            auto count = channelOffset + channelCount;

            channelData.resize(channelCount * totalPixels);
            size_t dataCount = 0;

            for (int32_t i = 0; i < totalPixels; i++) {
                for (int32_t j = channelOffset; j < count; j++) {
                    channelData[dataCount++] = level.data[i * channels + j];
                }
            }

            return channelData;

        }

        template<typename T>
        Ref<Image<T>> Image<T>::GetChannelImage(int32_t channelOffset,
            int32_t channelCount, int32_t mipLevel) const {

            auto& level = mipLevels[mipLevel];
            Image<T> image(level.width, level.height, channelCount);

            image.fileFormat = this->fileFormat;
            image.fileName = this->fileName;

            image.SetData(GetChannelData(channelOffset, channelCount, mipLevel));

            return CreateRef(image);

        }

        template<typename T>
        int32_t Image<T>::GetMipmapLevelCount() const {

            return int32_t(mipLevels.size());

        }

        template<typename T>
        void Image<T>::GenerateMipmap() {

            ivec2 dim = ivec2(width, height);

            // Max mipmap level count
            auto mipLevel = size_t(floor(log2(glm::max(float(dim.x),
                float(dim.y))))) + 1;

            mipLevels.resize(mipLevel);

            for (int32_t i = 0; i < mipLevels.size(); i++) {
                mipLevels[i].width = dim.x;
                mipLevels[i].height = dim.y;
                mipLevels[i].data.resize(dim.x * dim.y * channels);
                dim.x /= 2;
                dim.y /= 2;
            }

            for (int32_t i = 1; i < mipLevels.size(); i++) {
                dim.x /= 2;
                dim.y /= 2;
                if constexpr (std::is_same_v<T, uint8_t>) {
                    stbir_resize_uint8_generic(mipLevels[i - 1].data.data(), dim.x * 2,
                        dim.y * 2, dim.x * 2 * channels,
                        mipLevels[i].data.data(), dim.x,
                        dim.y, dim.x * channels, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
                else if constexpr (std::is_same_v<T, uint16_t>) {
                    stbir_resize_uint16_generic(mipLevels[i - 1].data.data(), dim.x * 2,
                        dim.y * 2, dim.x * 4 * channels,
                        mipLevels[i].data.data(), dim.x,
                        dim.y, dim.x * 2 * channels, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
                else if constexpr (std::is_same_v<T, float>) {
                    stbir_resize_float_generic(mipLevels[i - 1].data.data(), dim.x * 2,
                        dim.y * 2, dim.x * 8 * channels,
                        mipLevels[i].data.data(), dim.x,
                        dim.y, dim.x * 4 * channels, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
            }

        }

        template<typename T>
        void Image<T>::Resize(int32_t width, int32_t height) {

            std::vector<T> resizableData;

            if (mipLevels.size()) {
                resizableData = mipLevels[0].data;
            }

            mipLevels.resize(1);

            auto& level = mipLevels[0];

            level.width = width;
            level.height = height;
            level.data.resize(width * height * channels);

            if (resizableData.size()) {
                if constexpr (std::is_same_v<T, uint8_t>) {
                    stbir_resize_uint8_generic(resizableData.data(), this->width,
                        this->height, this->width * channels,
                        level.data.data(), width,
                        height, width * channels, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
                else if constexpr (std::is_same_v<T, uint16_t>) {
                    stbir_resize_uint16_generic(resizableData.data(), this->width,
                        this->height, this->width * channels * 2,
                        level.data.data(), width,
                        height, width * channels * 2, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
                else if constexpr (std::is_same_v<T, float>) {
                    stbir_resize_float_generic(resizableData.data(), this->width,
                        this->height, this->width * channels * 4,
                        level.data.data(), width,
                        height, width * channels * 4, channels, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
                        STBIR_COLORSPACE_LINEAR, nullptr);
                }
            }

            this->width = width;
            this->height = height;

        }

        template<typename T>
        void Image<T>::ApplyFilter(Filter filter) {

            auto tmp = *this;

            if (filter.IsSeparable()) {

                std::vector<float> weights;
                std::vector<float> offsets;

                filter.GetLinearized(&weights, &offsets, false);

                // Horizontal pass
                for (int32_t y = 0; y < height; y++) {
                    for (int32_t x = 0; x < width; x++) {

                        auto color = glm::vec4(0.0f);

                        for (int32_t i = 0; i < weights.size(); i++) {
                            int32_t off = x + int32_t(offsets[i]);
                            color += glm::vec4(Sample(off, y)) * weights[i];
                        }

                        if constexpr (std::is_integral_v<T>) {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(glm::round(color[i])));
                        }
                        else {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(color[i]));
                        }

                    }
                }

                // Vertical pass
                for (int32_t y = 0; y < height; y++) {
                    for (int32_t x = 0; x < width; x++) {

                        auto color = glm::vec4(0.0f);

                        for (int32_t i = 0; i < weights.size(); i++) {
                            int32_t off = y + (int32_t)offsets[i];
                            color += glm::vec4(tmp.Sample(x, off)) * weights[i];
                        }

                        if constexpr (std::is_integral_v<T>) {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(glm::round(color[i])));
                        }
                        else {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(color[i]));
                        }

                    }
                }

            }
            else {

                std::vector<std::vector<float>> weights;
                std::vector<std::vector<glm::ivec2>> offsets;

                filter.Get(&weights, &offsets);

                for (int32_t y = 0; y < height; y++) {
                    for (int32_t x = 0; x < width; x++) {

                        auto color = glm::vec4(0.0f);

                        for (uint32_t i = 0; i < weights.size(); i++) {
                            for (uint32_t j = 0; j < weights.size(); j++) {
                                int32_t offX = x + offsets[i][j].x;
                                int32_t offY = y + offsets[i][j].y;
                                color += glm::vec4(tmp.Sample(offX, offY)) * weights[i][j];
                            }
                        }

                        if constexpr (std::is_integral_v<T>) {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(glm::round(color[i])));
                        }
                        else {
                            for (int32_t i = 0; i < channels; i++)
                                tmp.SetData(x, y, i, T(color[i]));
                        }

                    }
                }

            }

        }

        template<typename T>
        void Image<T>::GammaToLinear() {

            auto& data = mipLevels[0].data;
            int32_t size = int32_t(data.size());

            auto invMaxPixelValue = 1.0f / MaxPixelValue();
            bool hasAlpha = channels == 4;

            for (int32_t i = 0; i < size; i++) {
                // Don't correct the alpha values
                if (hasAlpha && (i + 1) % 4 == 0) continue;

                float value = float(data[i]) * invMaxPixelValue;
                value = 0.76f * value * value + 0.24f * value * value * value;

                data[i] = T(value * MaxPixelValue());
            }

        }

        template<typename T>
        void Image<T>::FlipHorizontally() {

            auto flippedData = std::vector<T>(width * height * channels);
            int32_t idx = width * (height - 1) * channels + channels;

            for (int32_t i = 0; i < width * height * channels - channels; i++) {
                if (idx % (width * channels) == 0) {
                    idx = idx - 2 * width * channels;
                }

                flippedData[idx] = mipLevels[0].data[i];
                idx++;
            }

            mipLevels[0].data = flippedData;

        }

        template<typename T>
        void Image<T>::ExpandToChannelCount(int32_t channels, T fill) {

            auto oldChannels = this->channels;
            auto oldData = mipLevels[0].data;

            mipLevels[0].data.clear();

            this->channels = channels;
            Resize(width, height);

            auto& data = mipLevels[0].data;
            int32_t size = int32_t(data.size()) / channels;

            for (int32_t i = 0; i < size; i++) {
                for (int32_t j = 0; j < channels; j++) {
                    data[i * channels + j] = j < oldChannels ? oldData[i * oldChannels + j] : fill;
                }
            }

        }

        template<typename T>
        template<typename R>
        std::vector<R> Image<T>::ConvertData() {

            static_assert(std::is_same_v<R, uint8_t> || std::is_same_v<R, uint16_t> ||
                std::is_same_v<R, float> || std::is_same_v<R, float16>, "Unsupported conversion format");

            std::vector<R> convertedData;
            auto& data = GetData();

            for (auto value : data) {
                if constexpr (std::is_same_v<R, uint8_t>) {
                    T t = std::max(T(0), std::min(T(255), value));
                    convertedData.push_back(reinterpret_cast<R>(t));
                }
                else if constexpr (std::is_same_v<R, uint16_t>) {
                    T t = std::max(T(0), std::min(T(65535), value));
                    convertedData.push_back(reinterpret_cast<R>(t));
                }
                else if constexpr (std::is_same_v<R, float>) {
                    convertedData.push_back(reinterpret_cast<R>(value));
                }
                else if constexpr (std::is_same_v<R, float16>) {
                    float t = 0.0f;
                    if constexpr (std::is_same_v<T, float>) {
                        t = value;
                    }
                    else {
                        t = reinterpret_cast<float>(value);
                    }
                    convertedData.push_back(glm::detail::toFloat16(t));
                }
            }

            return convertedData;

        }

        template<typename T>
        inline float Image<T>::MaxPixelValue() const {

            if constexpr (std::is_same_v<T, uint8_t>) {
                return 255.0f;
            }
            else if constexpr (std::is_same_v<T, uint16_t>) {
                return 65535.0f;
            }
            else if constexpr (std::is_same_v<T, float>) {
                return 1.0f;
            }

        }

    }

}