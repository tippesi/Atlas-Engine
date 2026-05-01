#include "TerrainLoader.h"
#include "AssetLoader.h"
#include "MaterialLoader.h"
#include "resource/ResourceManager.h"
#include "../Log.h"

#include "../common/Path.h"

namespace Atlas {

    namespace Loader {

        void TerrainLoader::SaveTerrain(Ref<Terrain::Terrain> terrain, const std::string& filename) {

            auto fileStream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't write terrain file " + filename);
                return;
            }

            auto materials = terrain->storage->GetMaterials();

            // There don't have to be all materials
            int32_t count = 0;
            for (auto material : materials)
                if (material.IsValid())
                    count++;

            // Write file header in ASCII
            std::string header, body;

            header.append("AET ");
            header.append(std::to_string(count) + " ");
            header.append(std::to_string(terrain->rootNodeSideCount) + " ");
            header.append(std::to_string(terrain->LoDCount) + " ");
            header.append(std::to_string(terrain->patchSizeFactor) + " ");
            header.append(std::to_string(terrain->resolution) + " ");
            header.append(std::to_string(terrain->heightScale) + " ");
            header.append(std::to_string(terrain->translation.x) + " ");
            header.append(std::to_string(terrain->translation.y) + " ");
            header.append(std::to_string(terrain->translation.z) + " ");
            header.append(std::to_string(terrain->bakeResolution) + "\n");

            fileStream << header;

            body.append(std::to_string(terrain->tessellationFactor) + " ");
            body.append(std::to_string(terrain->tessellationSlope) + " ");
            body.append(std::to_string(terrain->tessellationShift) + " ");
            body.append(std::to_string(terrain->maxTessellationLevel) + " ");
            body.append(std::to_string(terrain->displacementDistance) + "\n");

            // Write all informations about the LoDs
            for (int32_t i = 0; i < terrain->LoDCount; i++) {
                body.append(std::to_string(terrain->GetLoDDistance(i)));
                if (i + 1 == terrain->LoDCount)
                    body.append("\n");
                else
                    body.append(" ");
            }

            // Write all material paths and store the materials
            auto terrainDir = Common::Path::GetDirectory(filename);

            count = 0;
            for (auto& material : materials) {
                if (material.IsValid()) {
                    body.append(std::to_string(count) + " " + material.GetResource()->path + "\n");
                }
                if (material.IsLoaded()) {
                    MaterialLoader::SaveMaterial(material.Get(), material.GetResource()->path);
                }
                count++;
            }

            fileStream << body;

            // Iterate over all LoD level
            for (int32_t i = 0; i < terrain->LoDCount; i++) {

                int32_t cellSideCount = (int32_t)sqrtf((float)terrain->storage->GetCellCount(i));

                auto isLeaf = i == terrain->LoDCount - 1;

                for (int32_t x = 0; x < cellSideCount; x++) {
                    for (int32_t y = 0; y < cellSideCount; y++) {

                        auto cell = terrain->storage->GetCell(x, y, i);

                        if (isLeaf) {
                            // fileStream.write((char*)cell->materialIndices, sizeof(cell->materialIndices));
                        }

                        // Here we assume that all cells are present
                        auto heightData = cell->heightField->GetData<uint16_t>();

                        fileStream.write(reinterpret_cast<char*>(heightData.data()), heightData.size() * 2);

                        auto data = cell->normalMap->GetData<uint8_t>();

                        fileStream.write(reinterpret_cast<char*>(data.data()), data.size());

                        data = cell->splatMap->GetData<uint8_t>();

                        fileStream.write(reinterpret_cast<char*>(data.data()), data.size());

                    }
                }

            }

            fileStream.close();

        }

        Ref<Terrain::Terrain> TerrainLoader::LoadTerrain(const std::string& filename, bool loadNodes) {

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't read terrain file " + filename);
            }

            std::string header, line;
            std::getline(fileStream, header);

            if (header.compare(0, 4, "AET ") != 0) {
                throw ResourceLoadException(filename, "File isn't a terrain file " + filename);
            }

            vec3 translation;
            size_t offset = 4;
            auto materialCount = ReadInt(" ", header, offset);
            auto rootNodeSideCount = ReadInt(" ", header, offset);
            auto LoDCount = ReadInt(" ", header, offset);
            auto patchSizeFactor = ReadInt(" ", header, offset);
            auto resolution = ReadFloat(" ", header, offset);
            auto heightScale = ReadFloat(" ", header, offset);
            translation.x = ReadFloat(" ", header, offset);
            translation.y = ReadFloat(" ", header, offset);
            translation.z = ReadFloat(" ", header, offset);
            auto bakeResolution = ReadInt("\r\n", header, offset);

            std::getline(fileStream, line);

            offset = 0;
            auto tessFactor = ReadFloat(" ", line, offset);
            auto tessSlope = ReadFloat(" ", line, offset);
            auto tessShift = ReadFloat(" ", line, offset);
            auto tessMaxLevel = ReadFloat(" ", line, offset);
            auto displacementDistance = ReadFloat("\r\n", line, offset);

            auto terrain = std::make_shared<Terrain::Terrain>(rootNodeSideCount, LoDCount,
                patchSizeFactor, resolution, heightScale);

            terrain->translation = translation;
            terrain->SetTessellationFunction(tessFactor, tessSlope, tessShift, tessMaxLevel);
            terrain->SetDisplacementDistance(displacementDistance);

            terrain->bakeResolution = bakeResolution;

            std::getline(fileStream, line);

            offset = 0;
            for (int32_t i = 0; i < LoDCount; i++) {
                float distance = 0.0f;
                if (i == LoDCount - 1)
                    distance = ReadFloat("\r\n", line, offset);
                else
                    distance = ReadFloat(" ", line, offset);
                terrain->SetLoDDistance(i, distance);
            }

            auto terrainDir = Common::Path::GetDirectory(filename);

            terrain->storage->BeginMaterialWrite();

            for (int32_t i = 0; i < materialCount; i++) {
                std::getline(fileStream, line);

                size_t offset = 0;
                auto slot = ReadInt(" ", line, offset);

                auto pos = line.find_last_of("\r\n");
                auto materialPath = line.substr(offset, pos - offset);

                auto material = ResourceManager<Material>::GetOrLoadResourceWithLoader(materialPath,
                    ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);

                if (material.IsLoaded())
                    terrain->storage->WriteMaterial(slot, material);
            }

            terrain->storage->EndMaterialWrite();

            fileStream.close();

            terrain->filename = filename;

            // Return early here, work is done
            if (!loadNodes)
                return terrain;

            std::vector<Terrain::TerrainStorageCell*> cells;

            for (int32_t depth = 0; depth < terrain->LoDCount; depth++) {
                int32_t cellSideCount = (int32_t)sqrtf((float)terrain->storage->GetCellCount(depth));

                for (int32_t x = 0; x < cellSideCount; x++) {
                    for (int32_t y = 0; y < cellSideCount; y++) {

                        Terrain::TerrainStorageCell* cell = terrain->storage->GetCell(x, y, depth);
                        cells.push_back(cell);                        

                    }
                }
            }

            Atlas::Loader::TerrainLoader::LoadStorageCells(terrain, cells, filename);            

            return terrain;

        }

        void TerrainLoader::LoadStorageCells(Ref<Terrain::Terrain> terrain, std::span<Terrain::TerrainStorageCell*> cells,
                const std::string& filename) {

            if (cells.empty())
                return;

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't read terrain file " + filename);
            }

            std::string header, body;
            std::getline(fileStream, header);

            if (header.compare(0, 4, "AET ") != 0) {
                throw ResourceLoadException(filename, "File isn't a terrain file " + filename);
            }

            auto position = header.find_first_of(' ', 4);
            int32_t materialCount = std::stoi(header.substr(4, position - 4));

            // Skip the body
            for (int32_t i = 0; i < materialCount + 2; i++)
                std::getline(fileStream, body);

            auto basePosition = fileStream.tellg();
            fileStream.seekg(0, std::ios::end);
            auto fileEndPosition = fileStream.tellg();
            fileStream.seekg(basePosition);

            auto tileResolution = 8 * terrain->patchSizeFactor + 1;

            // Height map + splat map
            auto nodeDataCount = (int64_t)tileResolution * tileResolution * 3;            
            auto normalDataResolution = int64_t(0);
            auto baseOffset = std::streamoff(basePosition);
            auto fileEndOffset = std::streamoff(fileEndPosition);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            Graphics::MemoryTransferManager transferManager(device, device->memoryManager);

            const size_t batchCount = 8;
            transferManager.BeginMultiTransfer();

            for (size_t i = 0; i < cells.size(); i++) {
                if (i % batchCount == 0 && i > 0) {
                    // Close old batch, create new one
                    transferManager.EndMultiTransfer();
                    transferManager.BeginMultiTransfer();
                }

                auto cell = cells[i];

                auto currPos = int64_t(0);
                auto nodeSize = int64_t(0);

                auto downsample = (int32_t)powf(2.0f, (float)terrain->LoDCount - 1.0f);
                auto tileSideCount = (int64_t)sqrtf((float)terrain->storage->GetCellCount(0));

                // Different resolutions for each LoD
                for (int32_t j = 0; j <= cell->LoD; j++) {
                    auto sizeFactor = int64_t(glm::min(downsample,
                        terrain->bakeResolution / (tileResolution - 1)));
                    normalDataResolution = int64_t(tileResolution - 1) * sizeFactor + 3;
                    nodeSize = nodeDataCount + normalDataResolution
                        * normalDataResolution * 4;

                    if (cell->LoD == j) {
                        currPos += (cell->x * tileSideCount + cell->y) * nodeSize;
                        break;
                    }

                    currPos += tileSideCount * tileSideCount * nodeSize;

                    downsample /= 2;
                    tileSideCount *= 2;
                }

                cell->storage = terrain->storage.get();

                auto cellDataOffset = baseOffset + std::streamoff(currPos);
                if (cellDataOffset < baseOffset || cellDataOffset + std::streamoff(nodeSize) > fileEndOffset) {
                    throw ResourceLoadException(filename, "Terrain file is truncated or corrupted: " + filename);
                }

                fileStream.seekg(cellDataOffset);
                if (!fileStream.good()) {
                    throw ResourceLoadException(filename, "Couldn't seek terrain file " + filename);
                }

                std::vector<uint16_t> heightFieldData(tileResolution * tileResolution);
                auto heightDataSize = std::streamsize(heightFieldData.size() * sizeof(uint16_t));
                auto& ret = fileStream.read(reinterpret_cast<char*>(heightFieldData.data()), heightDataSize);
                if (!ret || ret.gcount() != heightDataSize) {
                    throw ResourceLoadException(filename, "Couldn't read terrain height data from " + filename);
                }
                cell->heightField = CreateRef<Texture::Texture2D>(tileResolution, tileResolution,
                    VK_FORMAT_R16_UINT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
                cell->heightField->SetData(heightFieldData, &transferManager);

                Common::Image<uint8_t> image(normalDataResolution, normalDataResolution, 4);
                auto normalDataSize = std::streamsize(image.GetData().size());
                auto& ret = fileStream.read(reinterpret_cast<char*>(image.GetData().data()), normalDataSize);
                if (!ret || ret.gcount() != normalDataSize) {
                    throw ResourceLoadException(filename, "Couldn't read terrain normal data from " + filename);
                }
                cell->normalData = image.GetData();

                cell->normalMap = CreateRef<Texture::Texture2D>(normalDataResolution, normalDataResolution,
                    VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
                cell->normalMap->SetData(image.GetData(), &transferManager);

                std::vector<uint8_t> splatMapData(heightFieldData.size());
                auto splatDataSize = std::streamsize(splatMapData.size());
                auto& ret = fileStream.read(reinterpret_cast<char*>(splatMapData.data()), splatDataSize);
				if (!ret || ret.gcount() != splatDataSize) {
                    throw ResourceLoadException(filename, "Couldn't read terrain splat data from " + filename);
                }
                cell->splatMap = CreateRef<Texture::Texture2D>(tileResolution, tileResolution,
                    VK_FORMAT_R8_UINT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
                cell->splatMap->SetData(splatMapData, &transferManager);
                cell->materialIdxData = splatMapData;

                cell->heightData.resize(tileResolution * tileResolution);
                for (uint32_t j = 0; j < uint32_t(cell->heightData.size()); j++) {
                    if (heightFieldData[j] != 65535) {
                        cell->heightData[j] = (float)heightFieldData[j] / 65534.0f;
                    }
                    else {
                        cell->heightData[j] = FLT_MAX;
                    }
                }
                
                cell->isLoaded = true;
                cell->loadRequested = false;
            }

            transferManager.EndMultiTransfer();

            fileStream.close();

        }

        int32_t TerrainLoader::ReadInt(const char* ptr, std::string line, size_t& offset) {

            auto currOffset = offset;
            offset = line.find_first_of(ptr, currOffset + 1) + 1;
            return std::stoi(line.substr(currOffset, offset - currOffset - 1));

        }

        float TerrainLoader::ReadFloat(const char* ptr, std::string line, size_t& offset) {

            auto currOffset = offset;
            offset = line.find_first_of(ptr, currOffset + 1) + 1;
            return std::stof(line.substr(currOffset, offset - currOffset - 1));

        }

    }

}
