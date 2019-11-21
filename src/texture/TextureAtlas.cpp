#include "TextureAtlas.h"

#include "../shader/ShaderManager.h"

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

			int32_t width = 0, height = 0, layers = 0;

			for (auto& texture : textures) {
				layers++;
				width = texture->width > width ?
					texture->width : width;
				height = texture->height > height ?
					texture->height : height;
			}

			int32_t count = 0;

			for (auto& texture : textures) {
				slices[texture].layer = count++;
				slices[texture].offset = ivec2(0);
				slices[texture].size = ivec2(texture->width, texture->height);
			}

			texture = Texture2DArray(width, height, layers, AE_RGBA8);

			for (int32_t i = 0; i < textures.size(); i++) {

				auto data = textures[i]->GetData();

				// We need four channels (OpenGL restrictions for imageSamplers
				// in compute shaders)
				std::vector<uint8_t> convertedData;

				if (textures[i]->channels == 4) {

					convertedData = data;

				}
				else if (textures[i]->channels == 3) {

					auto pixelCount = textures[i]->width *
						textures[i]->height;

					convertedData.resize(pixelCount * 4);

					for (int32_t i = 0; i < pixelCount; i++) {

						convertedData[i * 4] = data[i * 3];
						convertedData[i * 4 + 1] = data[i * 3 + 1];
						convertedData[i * 4 + 2] = data[i * 3 + 2];
						convertedData[i * 4 + 3] = 0;

					}

				}

				texture.SetData(convertedData, 0, 0, i,
					textures[i]->width,	textures[i]->height, 1);

			}

		}

	}

}