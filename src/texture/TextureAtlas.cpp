#include "TextureAtlas.h"

#include "../shader/ShaderManager.h"
#include "../Framebuffer.h"
#include "../Log.h"

#include <algorithm>

namespace Atlas {

	namespace Texture {

		TextureAtlas::TextureAtlas(const TextureAtlas& that) {

			operator=(that);

		}

		TextureAtlas::TextureAtlas(std::vector<Texture2D*>& textures, int32_t padding,
			int32_t downscale) : padding(padding), downscale(downscale) {

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

			std::vector<TextureStructure> textureStructures;

			for (auto texture : textures) {
				textureStructures.emplace_back(
					TextureStructure{
					texture->width / downscale,
					texture->height / downscale,
					texture->channels,
					texture
					}
				);
			};

			for (auto& texture : textureStructures) {
				width = texture.width > width ?
					texture.width : width;
				height = texture.height > height ?
					texture.height : height;
				channels = texture.channels > channels ?
					texture.channels : channels;
			}

			std::sort(textureStructures.begin(), textureStructures.end(),
				[=](auto& texStruct1, auto& texStruct2) -> bool {
					auto pixelCount1 = texStruct1.width;
					auto pixelCount2 = texStruct2.width;

					if (pixelCount1 != pixelCount2) {
						return pixelCount1 > pixelCount2;
					}
					else {
						return texStruct1.height > texStruct2.height;
					}
				});

			// Approximation of total padding by assuming to stuff
			// the smallest texture over and over again.
			auto& smallest = textureStructures.back();
			auto smallestSize = glm::max(ivec2(smallest.width, smallest.height), glm::ivec2(1));
			ivec2 totalPadding = ivec2(width, height) / smallestSize * padding;

			// Add total padding to total size
			width += totalPadding.x;
			height += totalPadding.y;
			
			std::vector<std::map<Texture2D*, TextureAtlas::Slice>> levels;

			// This will create slices for four texture levels, each downsampled by a factor of two
			for (int32_t i = 0; i < 5; i++) {
				const auto levelSlices = CreateSlicesForAtlasLevel(textureStructures, i);
				levels.push_back(levelSlices);
			}

			int32_t sizedFormat;

			switch (channels) {
			case 1: sizedFormat = AE_R8; break;
			case 2: sizedFormat = AE_RG8; break;
			case 3: sizedFormat = AE_RGB8; break;
			default: sizedFormat = AE_RGBA8; break;
			}

			texture = Texture2DArray(width, height, layers, sizedFormat, GL_CLAMP_TO_EDGE, GL_LINEAR);

			// Copy all levels to the texture array (note that the order levels are added is important)
			for (auto& levelSlices : levels) {
				FillAtlas(levelSlices);
			}

		}

		std::map<Texture2D*, TextureAtlas::Slice> TextureAtlas::CreateSlicesForAtlasLevel(std::vector<TextureStructure> textures, int32_t level) {

			std::map<Texture2D*, TextureAtlas::Slice> levelSlices;

			int32_t downsampleFactor = pow(2, level);

			for (auto& texture : textures) {
				texture.width /= downsampleFactor;
				texture.height /= downsampleFactor;

				texture.width = glm::max(texture.width, 1);
				texture.height = glm::max(texture.height, 1);
			}

			// Reuse the offset for levels larger than zero
			// This reduces the layer by not creating a new one for each level
			bool reuseOffset = level != 0;
			// Try to add data to a lower layer for levels > 0
			if (reuseOffset) layers--;

			while (textures.size()) {

				offset = reuseOffset ? offset : ivec2(0);
				// Only reuse offset at first loop iteration
				reuseOffset = false;				

				size_t outerTex = 0;
				while (outerTex != textures.size()) {
					outerTex = textures.size();

					// Break here, we don't want to increment the y-offset twice when no data is left
					if (!textures.size())
						break;

					// Attempt to place the front most texture into the new roe
					auto textureStruct = textures.front();
					auto texture = textureStruct.texture;

					ivec2 size = ivec2(textureStruct.width, textureStruct.height);
					ivec2 remain = ivec2(width, height) - offset;

					// We need to increase the layer in case no space is left
					if (remain.x - size.x < 0 ||
						remain.y - size.y < 0) {
						break;
					}

					levelSlices[texture].layer = layers;
					levelSlices[texture].offset = offset;
					levelSlices[texture].size = size;

					textures.erase(textures.begin());

					offset.x += size.x + padding;

					size_t innerTex = 0;
					while (innerTex != textures.size()) {
						innerTex = textures.size();

						if (!textures.size() || offset.x == width)
							break;

						// Try to find a texture which might still fit in the
						// the current row
						for (size_t i = 0; i < textures.size(); i++) {

							textureStruct = textures[i];
							texture = textureStruct.texture;

							remain = ivec2(width, height) - offset;
							ivec2 texSize = ivec2(textureStruct.width, textureStruct.height);

							if (remain.x - texSize.x >= 0 &&
								remain.y - texSize.y >= 0 &&
								texSize.x <= size.x &&
								texSize.y <= size.y) {
								levelSlices[texture].layer = layers;
								levelSlices[texture].offset = offset;
								levelSlices[texture].size = texSize;
								offset.x += texSize.x + padding;
								textures.erase(textures.begin() + i);
								break;
							}

						}

					}

					offset.y += size.y + padding;
					offset.x = 0;

				}

				layers++;

			}

			return levelSlices;

		}

		void TextureAtlas::FillAtlas(std::map<Texture2D*, TextureAtlas::Slice> levelSlices) {

			Framebuffer readFramebuffer;
			Framebuffer writeFramebuffer;

			for (auto& key : levelSlices) {
				auto tex = key.first;
				auto slice = key.second;

				if (tex->channels == channels) {

					readFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, tex);
					writeFramebuffer.AddComponentTextureArray(GL_COLOR_ATTACHMENT0, &texture, slice.layer);

					readFramebuffer.Bind(GL_READ_FRAMEBUFFER);
					writeFramebuffer.Bind(GL_DRAW_FRAMEBUFFER);

					glBlitFramebuffer(0, 0, tex->width, tex->height,
						slice.offset.x, slice.offset.y, 
						slice.offset.x + slice.size.x, 
						slice.offset.y + slice.size.y,
						GL_COLOR_BUFFER_BIT, GL_LINEAR);

				}
				else {

					auto data = tex->GetData<uint8_t>();

					std::vector<uint8_t> convertedData;

					auto pixelCount = (size_t)tex->width *
						(size_t)tex->height;
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

					if (!convertedData.size())
						continue;

					// Downscale the texture data if it hasn't the right size
					if (slice.size.x != tex->width || slice.size.y != tex->height) {
						Common::Image<uint8_t> image(tex->width, tex->height, tex->channels);
						image.SetData(convertedData);
						image.Resize(slice.size.x, slice.size.y);
						convertedData = image.GetData();
					}

					texture.SetData(convertedData, slice.offset.x,
						slice.offset.y, slice.layer, slice.size.x,
						slice.size.y, 1);

				}

				slices[tex].push_back(slice);
			}

		}

	}

}