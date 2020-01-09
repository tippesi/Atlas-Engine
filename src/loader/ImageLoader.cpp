#include "ImageLoader.h"
#include "AssetLoader.h"
#include "../texture/Texture.h"
#include "../Log.h"

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
                Log::Error("Failed to load image " + filename);
				return image;
            }

            auto buffer = AssetLoader::GetFileContent(fileStream);

            fileStream.close();

			int32_t width, height, channels;
            auto data = stbi_load_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                    &width, &height, &channels, forceChannels);

            if (forceChannels > 0) {
                channels = forceChannels;
            }			

            if (colorSpaceConversion) {
                Texture::Texture::GammaToLinear(data, width, height, channels);
            }

			image = Common::Image8(width, height, channels);
			std::vector<uint8_t> imageData(width * height * channels);

            imageData.assign(data, data + width * height * channels);
			stbi_image_free(data);

			image.SetData(imageData);

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

            Log::Message("Loaded image " + filename);

            return image;

        }

		Common::Image16 ImageLoader::LoadImage16(std::string filename, bool colorSpaceConversion, int32_t forceChannels) {

			Common::Image16 image;
            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Failed to load image " + filename);
				return image;
            }

            auto buffer = AssetLoader::GetFileContent(fileStream);

            fileStream.close();

			int32_t width, height, channels;
            auto data = stbi_load_16_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
                    &width, &height, &channels, forceChannels);

            if (forceChannels > 0) {
                channels = forceChannels;
            }

            if (colorSpaceConversion) {
                Texture::Texture::GammaToLinear(data, width, height, channels);
            }

			image = Common::Image16(width, height, channels);
			std::vector<uint16_t> imageData(width * height * channels);

            imageData.assign(data, data + width * height * channels);
			stbi_image_free(data);

			image.SetData(imageData);

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

            Log::Message("Loaded image " + filename);

            return image;

        }

        void ImageLoader::SaveImage(Common::Image8 &image, std::string filename) {

            switch(image.fileFormat) {
				case AE_IMAGE_JPG: stbi_write_jpg(filename.c_str(), image.width, image.height,
					image.channels, image.GetData().data(), 100); break;
				case AE_IMAGE_BMP: stbi_write_bmp(filename.c_str(), image.width, image.height,
					image.channels, image.GetData().data()); break;
                case AE_IMAGE_PGM: SavePGM8(image, filename); break;
                case AE_IMAGE_PNG: stbi_write_png(filename.c_str(), image.width, image.height,
                        image.channels, image.GetData().data(), image.channels * image.width); break;
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
				Log::Error("Couldn't write image " + filename);
				return;
            }

            std::string header;

            // Create image header
            header.append("P2 ");
            header.append(std::to_string(image.width) + " ");
            header.append(std::to_string(image.height) + " ");
            header.append("255\n");

            imageFile << header;

			auto imageData = image.GetData();

			for (auto data : imageData) {
				imageFile << data;
				imageFile << " ";
			}

            imageFile.close();

        }

        void ImageLoader::SavePGM16(Common::Image16 &image, std::string filename) {

			auto imageFile = AssetLoader::WriteFile(filename, std::ios::out);

            if (!imageFile.is_open()) {
				Log::Error("Couldn't write image " + filename);
				return;
            }

            std::string header;

            // Create image header
            header.append("P2 ");
            header.append(std::to_string(image.width) + " ");
            header.append(std::to_string(image.height) + " ");
            header.append("65535\n");

            imageFile << header;

			auto imageData = image.GetData();

			for (auto data : imageData) {
				imageFile << data;
				imageFile << " ";
			}

            imageFile.close();

        }

	}

}