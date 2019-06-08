#include "ImageLoader.h"
#include "AssetLoader.h"
#include "../texture/Texture.h"

#include <algorithm>
#include <iostream>
#include <fstream>

//STB image library is declared(only once)
#define STB_IMAGE_IMPLEMENTATION
#include "libraries/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libraries/stb/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "libraries/stb/stb_image_resize.h"

namespace Atlas {

	namespace Loader {

		Common::Image8 ImageLoader::LoadImage(std::string filename, bool colorSpaceConversion, int32_t forceChannels) {

			Common::Image8 image;

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to load image %s", filename.c_str());
#endif
                throw AtlasException("Image couldn't be loaded");
            }

            auto buffer = AssetLoader::GetFileContent(fileStream);

            fileStream.close();

            auto data = stbi_load_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                    &image.width, &image.height, &image.channels, forceChannels);

            if (forceChannels > 0) {
                image.channels = forceChannels;
            }

            if (colorSpaceConversion) {
                Texture::Texture::GammaToLinear(data, image.width, image.height, image.channels);
            }

            image.data.assign(data, data + image.width * image.height * image.channels);
			stbi_image_free(data);

            auto fileFormatPosition = filename.find_last_of('.') + 1;
            auto fileFormat = filename.substr(fileFormatPosition, filename.length());

            std::transform(fileFormat.begin(), fileFormat.end(), fileFormat.begin(), ::tolower);

            if (fileFormat == "png") {
                image.fileFormat = AE_IMAGE_PNG;
            }
            else if (fileFormat == "jpg" || fileFormat == "jpeg") {
                image.fileFormat = AE_IMAGE_JPG;
            }
            else if (fileFormat == "bmp") {
                image.fileFormat = AE_IMAGE_BMP;
            }
            else if (fileFormat == "pgm") {
                image.fileFormat = AE_IMAGE_PGM;
            }

#ifdef AE_SHOW_LOG
            AtlasLog("Loaded image %s", filename.c_str());
#endif

            return image;

        }

		Common::Image16 ImageLoader::LoadImage16(std::string filename, bool colorSpaceConversion, int32_t forceChannels) {

			Common::Image16 image;

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to load image %s", filename.c_str());
#endif
                throw AtlasException("Image couldn't be loaded");
            }

            auto buffer = AssetLoader::GetFileContent(fileStream);

            fileStream.close();

            auto data = stbi_load_16_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                    &image.width, &image.height, &image.channels, forceChannels);

            if (forceChannels > 0) {
                image.channels = forceChannels;
            }

            if (colorSpaceConversion) {
                Texture::Texture::GammaToLinear(data, image.width, image.height, image.channels);
            }

            image.data.assign(data, data + image.width * image.height * image.channels);
			stbi_image_free(data);

            auto fileFormatPosition = filename.find_last_of('.') + 1;
            auto fileFormat = filename.substr(fileFormatPosition, filename.length());

            std::transform(fileFormat.begin(), fileFormat.end(), fileFormat.begin(), ::tolower);

            if (fileFormat == "png") {
                image.fileFormat = AE_IMAGE_PNG;
            }
            else if (fileFormat == "jpg" || fileFormat == "jpeg") {
                image.fileFormat = AE_IMAGE_JPG;
            }
            else if (fileFormat == "bmp") {
                image.fileFormat = AE_IMAGE_BMP;
            }
            else if (fileFormat == "pgm") {
                image.fileFormat = AE_IMAGE_PGM;
            }

#ifdef AE_SHOW_LOG
            AtlasLog("Loaded image %s", filename.c_str());
#endif

            return image;

        }

        void ImageLoader::SaveImage(Common::Image8 &image, std::string filename) {

            switch(image.fileFormat) {
                case AE_IMAGE_JPG: break;
                case AE_IMAGE_BMP: break;
                case AE_IMAGE_PGM: SavePGM8(image, filename); break;
                case AE_IMAGE_PNG: stbi_write_png(filename.c_str(), image.width, image.height,
                        image.channels, image.data.data(), image.channels * image.width); break;
                default: break;
            }

        }

        void ImageLoader::SaveImage16(Common::Image16 &image, std::string filename) {

            switch(image.fileFormat) {
                case AE_IMAGE_JPG: break;
                case AE_IMAGE_BMP: break;
                case AE_IMAGE_PGM: SavePGM16(image, filename); break;
                case AE_IMAGE_PNG: break;
                default: break;
            }

        }

        void ImageLoader::SavePGM8(Common::Image8 &image, std::string filename) {

			auto imageFile = AssetLoader::WriteFile(filename, std::ios::out);

            if (!imageFile.is_open()) {
                throw AtlasException("Couldn't write image");
            }

            std::string header;

            // Create image header
            header.append("P2 ");
            header.append(std::to_string(image.width) + " ");
            header.append(std::to_string(image.height) + " ");
            header.append("255\n");

            imageFile << header;

			for (auto data : image.data) {
				imageFile << data;
				imageFile << " ";
			}

            imageFile.close();

        }

        void ImageLoader::SavePGM16(Common::Image16 &image, std::string filename) {

			auto imageFile = AssetLoader::WriteFile(filename, std::ios::out);

            if (!imageFile.is_open()) {
                throw AtlasException("Couldn't write image");
            }

            std::string header;

            // Create image header
            header.append("P2 ");
            header.append(std::to_string(image.width) + " ");
            header.append(std::to_string(image.height) + " ");
            header.append("65535\n");

            imageFile << header;

			for (auto data : image.data) {
				imageFile << data;
				imageFile << " ";
			}

            imageFile.close();

        }

	}

}