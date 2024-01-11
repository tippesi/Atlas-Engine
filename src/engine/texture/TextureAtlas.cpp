#include "TextureAtlas.h"

#include "../Log.h"
#include "../graphics/GraphicsDevice.h"

#include <algorithm>

namespace Atlas {

    namespace Texture {

        TextureAtlas::TextureAtlas(const TextureAtlas& that) {

            operator=(that);

        }

        TextureAtlas::TextureAtlas(const std::set<Ref<Texture2D>>& textures, int32_t padding,
            int32_t downscale) : padding(padding), downscale(downscale) {

            Update(textures);

        }

        TextureAtlas& TextureAtlas::operator=(const TextureAtlas& that) {

            if (this != &that) {

                slices = that.slices;
                padding = that.padding;

                textureArray = that.textureArray;

            }

            return *this;

        }

        void TextureAtlas::Update(const std::set<Ref<Texture2D>>& textures) {

            if (!textures.size())
                return;

            std::vector<TextureStructure> textureStructures;
            this->textures = textures;

            // We can use raw pointers, as we have partial ownership of these textures,
            // thus they can't be deleted
            for (auto texture : textures) {
                textureStructures.emplace_back(
                    TextureStructure{
                    texture->width / downscale,
                    texture->height / downscale,
                    texture->channels,
                    texture.get()
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
            auto largest = textureStructures.front();
            ivec2 totalPadding = ivec2(glm::ceil(glm::log2(glm::vec2(float(largest.width), float(largest.height))))) * padding;

            // Add total padding to total size
            width += totalPadding.x;
            height += totalPadding.y;
            
            std::vector<std::map<Texture2D*, TextureAtlas::Slice>> levels;

            // This will create slices for four texture levels, each downsampled by a factor of two
            for (int32_t i = 0; i < 5; i++) {
                const auto levelSlices = CreateSlicesForAtlasLevel(textureStructures, i);
                levels.push_back(levelSlices);
            }

            VkFormat format;

            switch (channels) {
            case 1: format = VK_FORMAT_R8_UNORM; break;
            case 2: format = VK_FORMAT_R8G8_UNORM; break;
            case 3: format = VK_FORMAT_R8G8B8_UNORM; break;
            default: format = VK_FORMAT_R8G8B8A8_UNORM; break;
            }

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
            // Some device (like Apple M1) don't support RGB8_UNORM
            if (!graphicsDevice->CheckFormatSupport(VK_FORMAT_R8G8B8_UNORM,
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && channels == 3) {
                format = VK_FORMAT_R8G8B8A8_UNORM;
                channels = 4;
            }

            textureArray = Texture2DArray(width, height, layers, format,
                Wrapping::ClampToEdge, Filtering::Linear);

            auto commandList = graphicsDevice->GetCommandList(Graphics::GraphicsQueue, true);

            commandList->BeginCommands();

            commandList->ImageTransition(textureArray.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT);

            // Copy all levels to the texture array (note that the order levels are added is important)
            for (auto& levelSlices : levels) {
                FillAtlas(commandList, levelSlices);
            }

            commandList->ImageTransition(textureArray.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);

            commandList->EndCommands();
            graphicsDevice->FlushCommandList(commandList);

        }

        void TextureAtlas::Clear() {

            textureArray = Texture2DArray();

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

        void TextureAtlas::FillAtlas(Graphics::CommandList* commandList,
            std::map<Texture2D*, TextureAtlas::Slice> levelSlices) {

            for (auto& key : levelSlices) {
                auto tex = key.first;
                auto slice = key.second;

                auto prevLayout = tex->image->layout;
                auto prevAccessMask = tex->image->accessMask;
                commandList->ImageTransition(tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT);

                VkImageBlit blit = {};
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { int32_t(tex->width), int32_t(tex->height), 1};
                blit.srcSubresource.aspectMask = tex->image->aspectFlags;
                blit.srcSubresource.mipLevel = 0;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;

                blit.dstOffsets[0] = { slice.offset.x, slice.offset.y, 0 };
                blit.dstOffsets[1] = { slice.offset.x + slice.size.x,
                                       slice.offset.y + slice.size.y, 1};
                blit.dstSubresource.aspectMask = textureArray.image->aspectFlags;
                blit.dstSubresource.mipLevel = 0;
                blit.dstSubresource.baseArrayLayer = slice.layer;
                blit.dstSubresource.layerCount = 1;

                commandList->BlitImage(tex->image, textureArray.image, blit);

                commandList->ImageTransition(tex->image, prevLayout, prevAccessMask);

                slices[tex].push_back(slice);
            }

        }

    }

}