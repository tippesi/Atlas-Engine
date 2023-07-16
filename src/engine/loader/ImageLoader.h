#ifndef AE_IMAGELOADER_H
#define AE_IMAGELOADER_H

#include "../System.h"
#include "../common/Image.h"
#include "../common/Path.h"
#include "AssetLoader.h"
#include "../Log.h"

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <stb_image.h>
#include <stb_image_write.h>

namespace Atlas {

    namespace Loader {

        class ImageLoader {

        public:
            /**
             * Loads an image with 8 bits per channel.
             * @param filename The name of the image file.
             * @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
             * @param forceChannels The number of channels to be forced. Default is zero, which means no force.
             * @param maxImageResolution The maximum resolution on both axis of the image. If larger it gets resized.
             * @return An Image object with all the important data.
             */
            template<typename T>
            static Ref<Common::Image<T>> LoadImage(const std::string& filename, bool colorSpaceConversion = false, int32_t forceChannels = 0,
                int32_t maxImageResolution = 8192) {

                const auto normalizedFileName = Common::Path::Normalize(filename);

                Common::Image<T> image;
                auto fileStream = AssetLoader::ReadFile(normalizedFileName, std::ios::in | std::ios::binary);

                if (!fileStream.is_open()) {
                    Log::Error("Failed to load image " + normalizedFileName);
                    return CreateRef(image);
                }

                auto buffer = AssetLoader::GetFileContent(fileStream);

                fileStream.close();

                int32_t width, height, channels;
                void* data = nullptr;

                if constexpr (std::is_same_v<T, uint8_t>) {
                    data = static_cast<void*>(stbi_load_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                        &width, &height, &channels, forceChannels));
                }
                else if constexpr (std::is_same_v<T, uint16_t>) {
                    data = static_cast<void*>(stbi_load_16_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                        &width, &height, &channels, forceChannels));
                }
                else if constexpr (std::is_same_v<T, float>) {
                    data = static_cast<void*>(stbi_loadf_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                        &width, &height, &channels, forceChannels));
                }

                if (forceChannels > 0) {
                    channels = forceChannels;
                }

                image = Common::Image<T>(width, height, channels);
                std::vector<T> imageData(width * height * channels);

                imageData.assign(static_cast<T*>(data), static_cast<T*>(data) + width * height * channels);
                stbi_image_free(static_cast<T*>(data));

                image.SetData(imageData);

                if (image.width > maxImageResolution ||
                    image.height > maxImageResolution) {
                    width = width > maxImageResolution ? maxImageResolution : width;
                    height = height > maxImageResolution ? maxImageResolution : height;
                    image.Resize(width, height);
                }

                if (colorSpaceConversion) {
                    image.GammaToLinear();
                }

                auto fileFormatPosition = normalizedFileName.find_last_of('.') + 1;
                auto fileFormat = normalizedFileName.substr(fileFormatPosition, normalizedFileName.length());

                std::transform(fileFormat.begin(), fileFormat.end(), fileFormat.begin(), ::tolower);

                if (fileFormat == "png") {
                    image.fileFormat = Common::ImageFormat::PNG;
                }
                else if (fileFormat == "jpg" || fileFormat == "jpeg") {
                    image.fileFormat =Common::ImageFormat::JPG;
                }
                else if (fileFormat == "bmp") {
                    image.fileFormat = Common::ImageFormat::BMP;
                }
                else if (fileFormat == "pgm") {
                    image.fileFormat = Common::ImageFormat::PGM;
                }
                else if (fileFormat == "hdr") {
                    image.fileFormat = Common::ImageFormat::HDR;
                }

                image.fileName = normalizedFileName;

                Log::Message("Loaded image " + normalizedFileName);

                return CreateRef(image);

            }

            /**
             * Save an image to the hard drive.
             * @param image The image to be stored
             * @param filename The filename that the image should have
             * @note By changing the fileFormat in the Image object the output file changes as well.
             */
            template<typename T>
            static void SaveImage(Ref<Common::Image<T>>& image, const std::string& filename) {

                const auto normalizedFileName = Common::Path::Normalize(filename);

                std::ofstream imageStream;

                if (image->fileFormat == Common::ImageFormat::PGM) {
                    imageStream = AssetLoader::WriteFile(normalizedFileName, std::ios::out);
                }
                else {
                    imageStream = AssetLoader::WriteFile(normalizedFileName, std::ios::out | std::ios::binary);
                }

                if (!imageStream.is_open()) {
                    Log::Error("Couldn't write image " + normalizedFileName);
                    return;
                }

                auto lambda = [](void* context, void* data, int32_t size) {
                    auto imageStream = (std::ofstream*)context;

                    imageStream->write((char*)data, size);
                };

                if (image->fileFormat == Common::ImageFormat::JPG) {
                    if constexpr (std::is_same_v<T, uint8_t>) {
                        stbi_write_jpg_to_func(lambda, &imageStream, image->width,
                            image->height, image->channels, image->GetData().data(), 100);
                    }
                }
                else if (image->fileFormat == Common::ImageFormat::BMP) {
                    if constexpr (std::is_same_v<T, uint8_t>) {
                        stbi_write_bmp_to_func(lambda, &imageStream, image->width,
                            image->height, image->channels, image->GetData().data());
                    }
                }
                else if (image->fileFormat == Common::ImageFormat::PNG) {
                    if constexpr (std::is_same_v<T, uint8_t>) {
                        stbi_write_png_to_func(lambda, &imageStream, image->width, image->height,
                            image->channels, image->GetData().data(), image->channels * image->width);
                    }
                }
                else if (image->fileFormat == Common::ImageFormat::PGM) {
                    if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>) {
                        SavePGM(image, imageStream);
                    }
                }
                else if (image->fileFormat == Common::ImageFormat::HDR) {
                    if constexpr (std::is_same_v<T, float>) {
                        stbi_write_hdr_to_func(lambda, &imageStream, image->width,
                            image->height, image->channels, image->GetData().data());
                    }
                }

                imageStream.close();

            }

        private:
            template<typename T>
            static void SavePGM(Ref<Common::Image<T>>& image, std::ofstream& imageStream) {

                static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>,
                    "Unsupported PGM format. Supported are uint8_t, uint16_t");

                std::string header;

                // Create image header
                header.append("P2 ");
                header.append(std::to_string(image.width) + " ");
                header.append(std::to_string(image.height) + " ");

                if constexpr (std::is_same_v<T, uint8_t>) {
                    header.append("255\n");
                }
                else if constexpr (std::is_same_v<T, uint16_t>) {
                    header.append("65535\n");
                }

                imageStream << header;

                auto imageData = image.GetData();

                for (auto data : imageData) {
                    imageStream << data;
                    imageStream << " ";
                }

            }

        };

    }

}

#endif