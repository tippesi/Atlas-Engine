#include "TextureAtlas.h"

#include "../shader/ShaderManager.h"

#include <algorithm>

namespace Atlas {

	namespace Texture {

		TextureAtlas::TextureAtlas(const TextureAtlas& that) {

			operator=(that);

		}

		TextureAtlas::TextureAtlas(std::vector<Texture2D*>& textures, 
			int32_t padding) : padding(padding) {

			Update(textures);

		}

		TextureAtlas& TextureAtlas::operator=(const TextureAtlas& that) {

			if (this != &that) {

				slices = that.slices;
				padding = that.padding;

				texture = that.texture;

			}

			return *this;

		}

		void TextureAtlas::Update(std::vector<Texture2D*>& textures) {

			if (!textures.size())
				return;

			int32_t width = 0, height = 0, 
				layers = 0, channels = 0;

			for (auto& texture : textures) {
				width = texture->width > width ?
					texture->width : width;
				height = texture->height > height ?
					texture->height : height;
				channels = texture->channels > channels ?
					texture->channels : channels;
			}

			auto copy = textures;

			std::sort(copy.begin(), copy.end(),
				[=](Texture* tex1, Texture* tex2) -> bool {

					auto pixelCount1 = tex1->width;
					auto pixelCount2 = tex2->width;

					if (pixelCount1 != pixelCount2) {
						return pixelCount1 > pixelCount2;
					}
					else {
						return tex1->height > tex2->height;
					}

				});

			// Approximation of total padding by assuming to stuff
			// the smallest texture over and over again.
			auto smallest = copy[copy.size() - 1];
			auto smallestSize = ivec2(smallest->width, smallest->height);
			ivec2 totalPadding = ivec2(width, height) / smallestSize * padding;

			// Add total padding to total size
			width += totalPadding.x;
			height += totalPadding.y;

			while(copy.size()) {

				auto texture = copy[0];
				copy.erase(copy.begin());

				slices[texture].layer = layers;
				slices[texture].offset = ivec2(0);
				slices[texture].size = ivec2(texture->width, texture->height);

				ivec2 size = ivec2(texture->width, texture->height);
				ivec2 offset = ivec2(size.x + padding, 0);

				if (size.x == width && size.y == height ||
					!copy.size()) {
					layers++;
					continue;
				}

				size_t outerTex = 0;
				while (outerTex != copy.size()) {
					outerTex = copy.size();

					if (!copy.size())
						break;

					size_t innerTex = 0;
					while (innerTex != copy.size()) {
						innerTex = copy.size();

						if (!copy.size() || offset.x == width)
							break;

						for (size_t i = 0; i < copy.size(); i++) {

							auto texture = copy[i];

							ivec2 remain = ivec2(width, height) - offset;
							ivec2 texSize = ivec2(texture->width, texture->height);

							if (remain.x - texSize.x >= 0 &&
								remain.y - texSize.y >= 0 &&
								texSize.x <= size.x &&
								texSize.y <= size.y) {
								slices[texture].layer = layers;
								slices[texture].offset = offset;
								slices[texture].size = texSize;
								offset.x += texSize.x + padding;
								copy.erase(copy.begin() + i);
								break;
							}

						}

					}

					offset.y += size.y + padding;
					offset.x = 0;

				}

				layers++;

			}

			int32_t sizedFormat;

			switch (channels) {
			case 1: sizedFormat = AE_R8; break;
			case 2: sizedFormat = AE_RG8; break;
			case 3: sizedFormat = AE_RGB8; break;
			default: sizedFormat = AE_RGBA8; break;
			}

			texture = Texture2DArray(width, height, layers, sizedFormat);

			for (auto& key : slices) {
				auto tex = key.first;
				auto slice = key.second;

				auto data = tex->GetData();

				std::vector<uint8_t> convertedData;

				if (tex->channels == channels) {

					convertedData = data;

				}
				else {

					auto pixelCount = (size_t)slice.size.x * 
						(size_t)slice.size.y;
					auto texChannels = tex->channels;

					convertedData.resize(pixelCount * channels);

					for (size_t i = 0; i < pixelCount; i++) {

						for (int32_t j = 0; j < channels; j++) {
							if (j < texChannels) {
								convertedData[i * channels + j] =
									data[i * texChannels + j];
							}
							else {
								// Alpha channel should be always 1.0 (if there is no alpha channel)
								if (j == 3) {
									convertedData[i * channels + j] = 255;
								}
								else {
									convertedData[i * channels + j] = 0;
								}
							}
						}

					}

				}

				if (!convertedData.size())
					continue;

				texture.SetData(convertedData, slice.offset.x, 
					slice.offset.y, slice.layer, slice.size.x, 
					slice.size.y, 1);
			}

		}

	}

}