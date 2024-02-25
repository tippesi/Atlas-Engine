#include <algorithm>
#include "TerrainStorage.h"

namespace Atlas {

    namespace Terrain {

        TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LoDCount, float sideLength,
            int32_t materialResolution, int32_t materialCount) : rootNodeCount(rootNodeCount), 
            LoDCount(LoDCount), materialResolution(materialResolution), materialCount(materialCount) {

            cells.resize(LoDCount);
            LoDSideLengths = new int32_t[LoDCount];

            baseColorMaps = Atlas::Texture::Texture2DArray(materialResolution,
                materialResolution, materialCount, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            roughnessMaps = Atlas::Texture::Texture2DArray(materialResolution,
                materialResolution, materialCount, VK_FORMAT_R8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            aoMaps = Atlas::Texture::Texture2DArray(materialResolution,
                materialResolution, materialCount, VK_FORMAT_R8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            normalMaps = Atlas::Texture::Texture2DArray(materialResolution,
                materialResolution, materialCount, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            displacementMaps = Atlas::Texture::Texture2DArray(materialResolution,
                materialResolution, materialCount, VK_FORMAT_R8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);

            materials.resize(materialCount);

            for (int32_t i = 0; i < LoDCount; i++) {

                cells[i] = std::vector<TerrainStorageCell>(rootNodeCount * (int32_t)powf(4, (float)i), this);
                LoDSideLengths[i] = (int32_t)sqrtf((float)rootNodeCount * powf(4.0f, (float)i));

                for (int32_t x = 0; x < LoDSideLengths[i]; x++) {
                    for (int32_t y = 0; y < LoDSideLengths[i]; y++) {
                        cells[i][x * LoDSideLengths[i] + y].x = x;
                        cells[i][x * LoDSideLengths[i] + y].y = y;
                        cells[i][x * LoDSideLengths[i] + y].LoD = i;
                        cells[i][x * LoDSideLengths[i] + y].position = vec2((float)x, (float)y) * sideLength;
                    }
                }

                sideLength /= 2.0f;

            }

        }

        TerrainStorageCell* TerrainStorage::GetCell(int32_t x, int32_t y, int32_t LoD) {

            if (x < 0 || x >= LoDSideLengths[LoD] || y < 0 || y >= LoDSideLengths[LoD])
                return nullptr;

            return &cells[LoD][x * LoDSideLengths[LoD] + y];

        }

        int32_t TerrainStorage::GetCellCount(int32_t LoD) {

            return (int32_t)cells[LoD].size();

        }

        void TerrainStorage::BeginMaterialWrite() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue, true);

            commandList->BeginCommands();

        }

        void TerrainStorage::WriteMaterial(int32_t slot, Ref<Material> material) {

            materials[slot] = material;

            if (material->HasBaseColorMap()) {
                BlitImageToImageArray(material->baseColorMap->image, baseColorMaps.image, slot);
            }
            if (material->HasRoughnessMap()) {
                BlitImageToImageArray(material->roughnessMap->image, roughnessMaps.image, slot);
            }
            if (material->HasAoMap()) {
                BlitImageToImageArray(material->aoMap->image, aoMaps.image, slot);
            }
            if (material->HasNormalMap()) {
                BlitImageToImageArray(material->normalMap->image, normalMaps.image, slot);
            }
            if (material->HasDisplacementMap()) {
                BlitImageToImageArray(material->displacementMap->image, displacementMaps.image, slot);
            }

        }

        void TerrainStorage::EndMaterialWrite() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            commandList->GenerateMipMaps(baseColorMaps.image);
            commandList->GenerateMipMaps(roughnessMaps.image);
            commandList->GenerateMipMaps(aoMaps.image);
            commandList->GenerateMipMaps(normalMaps.image);
            commandList->GenerateMipMaps(displacementMaps.image);

            const auto dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            const auto dstAccess = VK_ACCESS_SHADER_READ_BIT;
            commandList->ImageTransition(baseColorMaps.image, dstLayout, dstAccess);
            commandList->ImageTransition(roughnessMaps.image, dstLayout, dstAccess);
            commandList->ImageTransition(aoMaps.image, dstLayout, dstAccess);
            commandList->ImageTransition(normalMaps.image, dstLayout, dstAccess);
            commandList->ImageTransition(displacementMaps.image, dstLayout, dstAccess);

            commandList->EndCommands();

            device->FlushCommandList(commandList);

        }

        void TerrainStorage::RemoveMaterial(int32_t slot, Ref<Material> material) {

            materials[slot] = nullptr;

        }

        std::vector<Ref<Material>> TerrainStorage::GetMaterials() {

            return materials;

        }

        void TerrainStorage::BlitImageToImageArray(Ref<Graphics::Image>& srcImage,
            Ref<Graphics::Image>& dstImage, int32_t slot) {

            VkImageBlit blit = {};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { int32_t(srcImage->width), int32_t(srcImage->height), 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.baseArrayLayer = slot;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { int32_t(dstImage->width), int32_t(dstImage->height), 1 };

            auto prevSrcLayout = srcImage->layout;
            auto prevSrcAccess = srcImage->accessMask;

            commandList->ImageTransition(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
            commandList->ImageTransition(dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);

            commandList->BlitImage(srcImage, dstImage, blit);

            if (prevSrcLayout != VK_IMAGE_LAYOUT_UNDEFINED && prevSrcLayout != VK_IMAGE_LAYOUT_PREINITIALIZED)
                commandList->ImageTransition(srcImage, prevSrcLayout, prevSrcAccess);

        }

    }

}