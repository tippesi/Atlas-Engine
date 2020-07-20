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

		Common::Image<uint8_t> ImageLoader::LoadImage(std::string filename, bool colorSpaceConversion, int32_t forceChannels) {			

			Common::Image<uint8_t> image;
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

			image = Common::Image<uint8_t>(width, height, channels);
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
			else if (fileFormat == "hdr") {
				image.fileFormat = AE_IMAGE_HDR;
			}

            Log::Message("Loaded image " + filename);

            return image;

        }

		Common::Image<uint16_t> ImageLoader::LoadImage16(std::string filename, bool colorSpaceConversion, int32_t forceChannels) {

            Common::Image<uint16_t> image;
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

			image = Common::Image<uint16_t>(width, height, channels);
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
			else if (fileFormat == "hdr") {
				image.fileFormat = AE_IMAGE_HDR;
			}

            Log::Message("Loaded image " + filename);

            return image;

        }

		Common::Image<float> ImageLoader::LoadImageFloat(std::string filename, int32_t forceChannels) {

			Common::Image<float> image;
			auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

			if (!fileStream.is_open()) {
				Log::Error("Failed to load image " + filename);
				return image;
			}

			auto buffer = AssetLoader::GetFileContent(fileStream);

			fileStream.close();

			int32_t width, height, channels;
			auto data = stbi_loadf_from_memory((unsigned char*)buffer.data(), (int32_t)buffer.size(),
				&width, &height, &channels, forceChannels);

			if (forceChannels > 0) {
				channels = forceChannels;
			}

			image = Common::Image<float>(width, height, channels);
			std::vector<float> imageData(width * height * channels);

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

        void ImageLoader::SaveImage(Common::Image<uint8_t>&image, std::string filename) {

            std::ofstream imageStream;

            if (image.fileFormat == AE_IMAGE_PGM) {
                imageStream = AssetLoader::WriteFile(filename, std::ios::out);
            }
            else {
                imageStream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);
            }

            if (!imageStream.is_open()) {
                Log::Error("Couldn't write image " + filename);
                return;
            }

            auto lambda = [](void* context, void* data, int32_t size) {
                auto imageStream = (std::ofstream*)context;

                imageStream->write((char*)data, size);
            };

            switch (image.fileFormat) {
            case AE_IMAGE_JPG: stbi_write_jpg_to_func(lambda, &imageStream, image.width,
                image.height, image.channels, image.GetData().data(), 100); break;
            case AE_IMAGE_BMP: stbi_write_bmp_to_func(lambda, &imageStream, image.width,
                image.height, image.channels, image.GetData().data()); break;
            case AE_IMAGE_PGM: SavePGM8(image, imageStream); break;
            case AE_IMAGE_PNG: stbi_write_png_to_func(lambda, &imageStream, image.width, image.height,
                image.channels, image.GetData().data(), image.channels * image.width); break;
            default: break;
            }

            imageStream.close();

        }

        void ImageLoader::SaveImage16(Common::Image<uint16_t> &image, std::string filename) {

            switch(image.fileFormat) {
                case AE_IMAGE_JPG: break;
                case AE_IMAGE_BMP: break;
                case AE_IMAGE_PGM: SavePGM16(image, filename); break;
                case AE_IMAGE_PNG: break;
                default: break;
            }

        }

		void ImageLoader::SaveImageFloat(Common::Image<float>& image, std::string filename) {

			std::ofstream imageStream;

			imageStream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

			if (!imageStream.is_open()) {
				Log::Error("Couldn't write image " + filename);
				return;
			}

			auto lambda = [](void* context, void* data, int32_t size) {
				auto imageStream = (std::ofstream*)context;

				imageStream->write((char*)data, size);
			};

			switch (image.fileFormat) {
			case AE_IMAGE_HDR: stbi_write_hdr_to_func(lambda, &imageStream, image.width,
				image.height, image.channels, image.GetData().data()); break;
			default: break;
			}

			imageStream.close();

		}

        void ImageLoader::SavePGM8(Common::Image<uint8_t>&image, std::ofstream& imageStream) {

            std::string header;

            // Create image header
            header.append("P2 ");
            header.append(std::to_string(image.width) + " ");
            header.append(std::to_string(image.height) + " ");
            header.append("255\n");

            imageStream << header;

			auto imageData = image.GetData();

			for (auto data : imageData) {
                imageStream << data;
                imageStream << " ";
			}

        }

        void ImageLoader::SavePGM16(Common::Image<uint16_t>&image, std::string filename) {

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