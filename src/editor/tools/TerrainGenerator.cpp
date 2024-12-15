#include "TerrainGenerator.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImguiExtension/UiElements.h>
#include <tools/TerrainTool.h>
#include <common/NoiseGenerator.h>
#include <Notifications.h>
#include <Singletons.h>
#include <algorithm>

namespace Atlas::Editor {

    using namespace ImguiExtension;

    TerrainGenerator::TerrainGenerator() {

        previewHeightImg = CreateRef<Common::Image<uint16_t>>(previewSize, previewSize, 1);
        previewMoistureImg = CreateRef<Common::Image<uint16_t>>(previewSize, previewSize, 1);
        previewBiomeImg = CreateRef<Common::Image<uint8_t>>(previewSize, previewSize, 4);

        previewHeightMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        previewMoistureMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        previewBiomeMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R8G8B8A8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        newPreviewHeightMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        newPreviewMoistureMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        newPreviewBiomeMap = Texture::Texture2D(previewSize, previewSize, VK_FORMAT_R8G8B8A8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        // Set this first since it will be swapped out
        newPreviewHeightMap.SetData(previewHeightImg->GetData());
        newPreviewMoistureMap.SetData(previewMoistureImg->GetData());
        newPreviewBiomeMap.SetData(previewBiomeImg->GetData());

        auto amplitude = 1.0f;

        for (uint8_t i = 0; i < 8; i++) {
            heightAmplitudes.push_back(amplitude);
            moistureAmplitudes.push_back(amplitude);
            amplitude /= 2.0f;
        }

    }

    TerrainGenerator::~TerrainGenerator() {

        JobSystem::Wait(heightMapUpdateJob);
        JobSystem::Wait(previewMapGenerationJob);

    }

    void TerrainGenerator::Update() {


    }

    void TerrainGenerator::Render(ResourceHandle<Terrain::Terrain>& terrain) {

        const float padding = 8.0f;
        auto imguiWrapper = Singletons::imguiWrapper;

        textureSelectionPanel.Reset();
        materialSelectionPanel.Reset();

        GeneratePreviews();

        if (heightMap.IsLoaded() && heightMap->width != heightMap->height) {
            Notifications::Push({ "Texture isn't square. Need a texture with equal amount of pixel on each axis" });
            heightMap.Reset();
            return;
        }

        ImGui::PushID("Generator");

        auto width = ImGui::GetContentRegionAvail().x;

        ImGui::InputTextWithHint("Name", "Type name", &name);

        ImGui::Separator();
        ImGui::Text("Heightmap");

        ImGui::RadioButton("Generate new", &heightMapSelection, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Load file", &heightMapSelection, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Use existing terrain", &heightMapSelection, 2);

        UIElements::TextureView(imguiWrapper, &previewHeightMap, width / 2.0f);
        ImGui::SetItemTooltip("Higher octaves have a higher frequency, which might not be visible in the preview\n \
            Use the sliders to change the strength of an octave");

        if (heightMapSelection == 0) {
            const char* resolutions[] = { "512x512", "1024x1024", "2048x2048",
                "4096x4096", "8192x8192" };

            ImGui::Combo("Resolution", &resolutionSelection, resolutions, IM_ARRAYSIZE(resolutions));

            int32_t count = 0;
            for (auto& amplitude : heightAmplitudes) {
                ImGui::SliderFloat(("Octave" + std::to_string(count++) + "##0").c_str(), &amplitude,
                    0.0f, 1.0f, "%.3f");
            }

            ImGui::SliderFloat("Exponent", &heightExp, 0.01f, 10.0f, "%.3f");
            ImGui::SliderInt("Seed##0", &heightSeed, 0, 100);
        }
        else if (heightMapSelection == 1) {
            bool resourceChanged = false;
            heightMap = textureSelectionPanel.Render(heightMap, resourceChanged);

            // This is also done when the terrain generator is deserialized again
            if (heightMap.IsLoaded() && resourceChanged || heightMap.IsLoaded() && !heightMapImage) {
                UpdateHeightmapFromFile();
            }
        }
        else if (heightMapSelection == 2) {
            // Needs to be done in an async way in the future
            if (terrain.IsLoaded())
                UpdateHeightmapFromTerrain(terrain);
        }

        ImGui::Separator();

        ImGui::Text("General settings");
        ImGui::SliderInt("Number of LODs", &LoDCount, 1, 8);
        ImGui::SliderInt("Patch size", &patchSize, 1, 32);
        ImGui::DragFloat("Resolution", &resolution, 0.01f, 0.1f, 4.0f);
        ImGui::DragFloat("Height", &height, 0.25f, 1.0f, 2000.0f);

        std::string buttonText = advanced ? "Advanced" : "Standard";
        if (ImGui::Button(buttonText.c_str(), ImVec2(width, 0.0f))) {
            advanced = !advanced;
        }

        ImGui::Separator();

        if (!advanced) {
            ImGui::Text("Material");
            selectedMaterial = materialSelectionPanel.Render(selectedMaterial);
        }
        else {
            ImGui::Text("Moisture");

            UIElements::TextureView(imguiWrapper, &previewMoistureMap, width / 2.0f);
            ImGui::SetItemTooltip("Higher octaves have a higher frequency, which might not be visible in the preview\n \
                    Use the sliders to change the strength of an octave");

            int32_t count = 0;
            for (auto& amplitude : moistureAmplitudes) {
                ImGui::SliderFloat(("Octave" + std::to_string(count++) + "##1").c_str(), &amplitude,
                    0.0f, 1.0f, "%.3f");
            }

            ImGui::SliderInt("Seed##1", &moistureSeed, 0, 100);

            ImGui::Separator();
            ImGui::Text("Materials");

            auto region = ImGui::GetContentRegionAvail();
            auto lineHeight = ImGui::GetTextLineHeight();
            auto deleteButtonSize = ImVec2(lineHeight, lineHeight);

            auto& deleteIcon = Singletons::icons->Get(IconType::Delete);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&deleteIcon);

            int32_t matCount = 0;
            int32_t loopCount = 0;
            for (auto& [material, color] : materials) {
                ImGui::PushID(loopCount);

                auto materialName = material.IsValid() ? material.GetResource()->GetFileName() : "No material " + std::to_string(loopCount);

                auto treeNodeSize = region.x - (material.IsValid() ? deleteButtonSize.x + 2.0f * padding : 0.0f);
                ImGui::SetNextItemWidth(treeNodeSize);
                bool open = ImGui::TreeNode(materialName.c_str());
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                    material.Reset();
                }
                ImGui::PopStyleColor();

                if (open) {
                    ImGui::ColorEdit3("Material color", glm::value_ptr(color));
                    material = materialSelectionPanel.Render(material);
                    if (material.IsLoaded()) {
                        materialPanel.Render(imguiWrapper, material.Get(), [&](ResourceHandle<Texture::Texture2D> texture) {
                            return textureSelectionPanel.Render(texture);
                            });
                    }
                    ImGui::TreePop();
                }
                loopCount++;

                ImGui::PopID();
            }

            if (ImGui::Button("Add material", ImVec2(-FLT_MIN, 0.0f))) {
                materials.push_back({ ResourceHandle<Material>(), vec3(0.0f) });
            }

            ImGui::Separator();
            ImGui::Text("Biomes");

            UIElements::TextureView(imguiWrapper, &previewBiomeMap, width / 2.0f);
            ImGui::SetItemTooltip("Add materials to get started with the biome editing.\n"
                "Elevation biomes change the material based on the elevation of the height map.\n"
                "Moisture biomes change the material within an elevation biome based on the moisture map.");

            auto mats = materials;
            std::vector<std::string> materialNames;
            std::vector<const char*> pointer;

            for (auto& [material, color] : mats) {
                auto materialName = material.IsValid() ? material.GetResource()->GetFileName() : "No material " + std::to_string(loopCount);
                materialNames.push_back(materialName);
            }

            for (auto& materialName : materialNames) {
                pointer.push_back(materialName.c_str());
            }

            ImGui::Combo("Default material", &materialSelection,
                pointer.data(), pointer.size());
            if ((size_t)materialSelection < materials.size()) {
                selectedMaterial = materials[materialSelection].first;
            }

            int32_t eleBiomeCount = 0;
            int32_t deleteElevationElement = -1;
            for (auto& eleBiome : elevationBiomes) {
                ImGui::PushID(eleBiomeCount);

                auto eleRegion = ImGui::GetContentRegionAvail();

                bool eleOpen = ImGui::TreeNode(("Elevation biome " + std::to_string(eleBiomeCount++)).c_str());

                ImGui::SameLine();

                ImGui::SetCursorPosX(eleRegion.x - deleteButtonSize.x - padding);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                    deleteElevationElement = eleBiomeCount - 1;
                }
                ImGui::PopStyleColor();

                if (eleOpen) {
                    ImGui::DragFloat("Elevation", &eleBiome.elevation, 0.001f, 0.0f, 1.0f);
                    ImGui::RadioButton("Larger", &eleBiome.less, 0);
                    ImGui::SameLine();
                    UIElements::Tooltip("Everything larger than the biome elevation will be painted with the biomes materials");
                    ImGui::SameLine();
                    ImGui::RadioButton("Less", &eleBiome.less, 1);
                    ImGui::SameLine();
                    UIElements::Tooltip("Everything smaller than the biome elevation will be painted with the biomes materials");
                    if (eleBiome.materialIdx >= 0) {
                        ImGui::Combo("Material", &eleBiome.materialIdx, pointer.data(), pointer.size());
                    }
                    else {
                        if (ImGui::Button("Add material", ImVec2(-FLT_MIN, 0.0f))) {
                            eleBiome.materialIdx = 0;
                        }
                    }

                    if (ImGui::TreeNode("Moisture biomes")) {
                        int32_t moiBiomeCount = 0;
                        for (auto& moiBiome : eleBiome.moistureBiomes) {
                            if (ImGui::TreeNode(("Moisture biome " + std::to_string(moiBiomeCount++)).c_str())) {
                                ImGui::DragFloat("Moisture", &moiBiome.moisture, 0.001f, 0.0f, 1.0f);
                                ImGui::Combo("Material", &moiBiome.materialIdx, pointer.data(), pointer.size());
                                ImGui::TreePop();
                            }
                        }
                        if (ImGui::Button("Add moisture biome", ImVec2(-FLT_MIN, 0.0f))) {
                            MoistureBiome biome;
                            biome.materialIdx = 0;
                            biome.moisture = 0.1f;
                            eleBiome.moistureBiomes.push_back(biome);
                        }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Slope biomes")) {
                        int32_t sloBiomeCount = 0;
                        for (auto& sloBiome : eleBiome.slopeBiomes) {
                            if (ImGui::TreeNode(("Slope biome " + std::to_string(sloBiomeCount++)).c_str())) {
                                ImGui::DragFloat("Slope", &sloBiome.slope, 0.001f, 0.0f, 1.0f);
                                ImGui::Combo("Material", &sloBiome.materialIdx, pointer.data(), pointer.size());
                                ImGui::TreePop();
                            }
                        }
                        if (ImGui::Button("Add slope biome", ImVec2(-FLT_MIN, 0.0f))) {
                            SlopeBiome biome;
                            biome.materialIdx = 0;
                            biome.slope = 0.5f;
                            eleBiome.slopeBiomes.push_back(biome);
                        }
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            if (ImGui::Button("Add elevation biome", ImVec2(-FLT_MIN, 0.0f))) {
                ElevationBiome biome;
                biome.elevation = 0.1f;
                biome.materialIdx = -1;
                biome.less = 0;
                elevationBiomes.push_back(biome);
            }

            if (ImGui::Button("Sort biomes", ImVec2(-FLT_MIN, 0.0f))) {
                elevationBiomes = SortBiomes();
            }

            ImGui::SetItemTooltip("Sorts the biome based on a biome function. After sorting, all\n"
                "biomes get evaluated from top to bottom.");

            if (deleteElevationElement >= 0)
                elevationBiomes.erase(elevationBiomes.begin() + deleteElevationElement);

        }

        if (ImGui::Button("Generate", ImVec2(width, 0.0f))) {

            if (name.empty()) {
                Notifications::Push({ "Terrain name is missing!", vec3(1.0f, 0.0f, 0.0f) });
            }
            else if (!materials.size() && advanced) {
                Notifications::Push({ "At least one material is required!", vec3(1.0f, 0.0f, 0.0f) });
            }
            else if (!selectedMaterial.IsLoaded() && !advanced) {
                Notifications::Push({ "At least one material is required!", vec3(1.0f, 0.0f, 0.0f) });
            }
            else if (heightMapSelection && !heightMapImage->HasData()) {
                Notifications::Push({ "No heightmap loaded!", vec3(1.0f, 0.0f, 0.0f) });
            }
            else {
                Singletons::blockingOperation->Block("Generating terrain. Please wait...", [&]() {
                    Generate();
                    successful = true;
                    });
            }

        }

        ImGui::PopID();

    }

    ResourceHandle<Terrain::Terrain> TerrainGenerator::GetTerrain() {

        if (!successful || !Singletons::blockingOperation->job.HasFinished())
            return ResourceHandle<Terrain::Terrain>();

        auto path = "terrains/" + name + "/" + name + ".aeterrain";
        auto handle = ResourceManager<Terrain::Terrain>::GetResource(path);
        successful = false;

        return handle;

    }

    void TerrainGenerator::UpdateHeightmapFromTerrain(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!heightMapUpdateJob.HasFinished())
            return;

        std::swap(newHeightMapImage, heightMapImage);

        // Need to have copies of the images
        JobSystem::Execute(heightMapUpdateJob,
            [terrain = terrain, this](JobData&) mutable {
                Tools::TerrainTool::LoadMissingCells(terrain.Get(), terrain.GetResource()->path);
                newHeightMapImage = Tools::TerrainTool::GenerateHeightMap(terrain.Get());
            });

    }

    void TerrainGenerator::UpdateHeightmapFromFile() {

        if (heightMap.IsLoaded()) {
            heightMapImage = Loader::ImageLoader::LoadImage<uint16_t>(heightMap.GetResource()->path, false, 1);
        }

    }

    void TerrainGenerator::GenerateHeightAndMoistureImages(Ref<Common::Image<uint16_t>>& heightImg,
        Ref<Common::Image<uint16_t>>& moistureImg) {

        if (heightMapSelection == 0) {
            int32_t heightMapResolution = 128;
            switch (resolutionSelection) {
            case 0: heightMapResolution = 512; break;
            case 1: heightMapResolution = 1024; break;
            case 2: heightMapResolution = 2048; break;
            case 3: heightMapResolution = 4096; break;
            case 4: heightMapResolution = 8192; break;
            }

            heightImg = CreateRef<Common::Image<uint16_t>>(heightMapResolution,
                heightMapResolution, 1);
            Common::NoiseGenerator::GeneratePerlinNoise2DParallel(*heightImg, heightAmplitudes,
                (uint32_t)heightSeed, JobPriority::High, heightExp);
        }
        else {
            if (!heightMapImage)
                UpdateHeightmapFromFile();

            heightImg = heightMapImage;
        }

        moistureImg = CreateRef<Common::Image<uint16_t>>(heightImg->width / 2,
            heightImg->height / 2, 1);
        Common::NoiseGenerator::GeneratePerlinNoise2DParallel(*moistureImg, moistureAmplitudes,
            (uint32_t)moistureSeed, JobPriority::High);

    }

    void TerrainGenerator::GetBiomeIndicators(float x, float y, Common::Image<uint16_t>& heightImg, 
            Common::Image<uint16_t>& moistureImg, float scale, float& height, float& slope, float& moisture) {

        auto xTex = 1.0f / (float)heightImg.width;
        auto yTex = 1.0f / (float)heightImg.height;
        float heightL = (float)heightImg.SampleBilinear(x - xTex, y).r * scale / 65535.0f;
        float heightR = (float)heightImg.SampleBilinear(x + xTex, y).r * scale / 65535.0f;
        float heightD = (float)heightImg.SampleBilinear(x, y - yTex).r * scale / 65535.0f;
        float heightU = (float)heightImg.SampleBilinear(x, y + yTex).r * scale / 65535.0f;

        auto normal = glm::normalize(glm::vec3(heightL - heightR, 1.0f,
            heightD - heightU));

        height = (float)heightImg.SampleBilinear(x, y).r / 65535.0f;
        moisture = (float)moistureImg.SampleBilinear(x, y).r / 65535.0f;
        slope = glm::dot(normal, vec3(0.0f, 1.0f, 0.0f));

    }

    TerrainGenerator::Biome TerrainGenerator::GetBiome(float x, float y, std::vector<ElevationBiome>& biomes,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, float scale) {

        Biome biome;

        float e, s, m;
        GetBiomeIndicators(x, y, heightImg, moistureImg, scale, e, s, m);

        for (const auto& eleBiome : biomes) {
            if (eleBiome.less) {
                if (e >= eleBiome.elevation || eleBiome.materialIdx >= materials.size()) {
                    continue;
                }

                for (const auto& moiBiome : eleBiome.moistureBiomes) {
                    if (m >= moiBiome.moisture || moiBiome.materialIdx >= materials.size()) {
                        continue;
                    }
                    biome = moiBiome;
                    if (IsBiomeValid(biome))
                        break;
                }

                for (const auto& sloBiome : eleBiome.slopeBiomes) {
                    if (s >= sloBiome.slope || sloBiome.materialIdx >= materials.size()) {
                        continue;
                    }
                    biome = sloBiome;
                    if (IsBiomeValid(biome))
                        break;
                }

                if (IsBiomeValid(biome))
                    break;
                biome = eleBiome;
                if (IsBiomeValid(biome))
                    break;
            }
            else {
                if (e <= eleBiome.elevation || eleBiome.materialIdx >= materials.size()) {
                    continue;
                }

                for (const auto& moiBiome : eleBiome.moistureBiomes) {
                    if (m >= moiBiome.moisture || moiBiome.materialIdx >= materials.size()) {
                        continue;
                    }
                    biome = moiBiome;
                    if (IsBiomeValid(biome))
                        break;
                }

                for (const auto& sloBiome : eleBiome.slopeBiomes) {
                    if (s >= sloBiome.slope || sloBiome.materialIdx >= materials.size()) {
                        continue;
                    }
                    biome = sloBiome;
                    if (IsBiomeValid(biome))
                        break;
                }

                if (IsBiomeValid(biome))
                    break;

                biome = eleBiome;
                if (IsBiomeValid(biome))
                    break;
            }
        }

        if (IsBiomeValid(biome)) {
            return biome;
        }

        return biome;

    }

    void TerrainGenerator::Generate() {

        auto path = "terrains/" + name + "/" + name + ".aeterrain";
        auto handle = ResourceManager<Terrain::Terrain>::GetResource(path);

        Ref<Common::Image<uint16_t>> heightImage, moistureImage;
        GenerateHeightAndMoistureImages(heightImage, moistureImage);

        Ref<Terrain::Terrain> terrain = nullptr;

        if (!advanced) {
            terrain = Tools::TerrainTool::GenerateTerrain(*heightImage, 1,
                LoDCount, patchSize, resolution, height, selectedMaterial);
        }
        else {
            std::vector<ResourceHandle<Material>> mats;
            Common::Image<uint8_t> splatImage(heightImage->width,
                heightImage->height, 1);

            for (auto mat : materials) {
                mats.push_back(mat.first);
            }

            auto biomes = SortBiomes();

            // Need to do this because we want the slopes to be computed as in the preview image
            auto scale = height * (float)splatImage.width / (float)previewHeightImg->width;

            // Making this multi-threaded doesn't help much :(
            for (int32_t y = 0; y < splatImage.height; y++) {
                for (int32_t x = 0; x < splatImage.width; x++) {
                    auto fx = (float)x / (float)splatImage.width;
                    auto fy = (float)y / (float)splatImage.height;

                    auto biome = GetBiome(fx, fy, biomes, *heightImage,
                        *moistureImage, scale);

                    std::pair<ResourceHandle<Material>, vec3> pair;
                    if (biome.materialIdx < 0) {
                        // Try to use the default material
                        for (auto mat : materials) {
                            if (mat.first == selectedMaterial) {
                                pair = mat;
                                break;
                            }
                        }
                    }
                    else {
                        pair = materials[biome.materialIdx];
                    }

                    const auto& mat = pair.first;
                    uint8_t index = 0;

                    for (auto material : mats) {
                        if (material == mat)
                            break;
                        index++;
                    }
                    splatImage.SetData(x, y, 0, index);
                }
            };

            terrain = Tools::TerrainTool::GenerateTerrain(*heightImage, splatImage, 1,
                LoDCount, patchSize, resolution, height, mats);

        }

        terrain->filename = name;


        if (!handle.IsLoaded()) {
            handle = ResourceManager<Terrain::Terrain>::AddResource(path, terrain);

            // Only save when there is nothing on the disk yet
            Loader::TerrainLoader::SaveTerrain(terrain, path);
        }
        else {
            terrain->translation = handle->translation;
            // Need the mark the terrain as in-editing, otherwise data gets unloaded and reset from the hard disk
            terrain->storage->inEditing = true;

            handle.GetResource()->Swap(terrain);
        }
    }

    void TerrainGenerator::GeneratePreviews() {

        if (!previewMapGenerationJob.HasFinished())
            return;

        std::swap(newPreviewHeightMap, previewHeightMap);
        std::swap(newPreviewMoistureMap, previewMoistureMap);
        std::swap(newPreviewBiomeMap, previewBiomeMap);

        auto biomes = SortBiomes();

        // Need to have copies of the images
        JobSystem::Execute(previewMapGenerationJob,
            [previewHeightImg = *previewHeightImg, previewMoistureImg = *previewMoistureImg,
            previewBiomeImg = *previewBiomeImg, biomes = biomes, heightAmplitudes = heightAmplitudes,
            moistureAmplitudes = moistureAmplitudes, heightSeed = heightSeed, moistureSeed = moistureSeed,
            heightExp = heightExp, heightMapImage = heightMapImage, heightMapSelection = heightMapSelection, this](JobData&) mutable {

                if (heightMapSelection == 0 || !heightMapImage) {
                    Common::NoiseGenerator::GeneratePerlinNoise2D(previewHeightImg, heightAmplitudes,
                        (uint32_t)heightSeed, heightExp);

                    newPreviewHeightMap.SetData(previewHeightImg.GetData());
                }
                else {
                    for (int32_t y = 0; y < previewHeightImg.height; y++) {
                        for (int32_t x = 0; x < previewHeightImg.width; x++) {
                            auto fx = float(x) / float(previewBiomeImg.width);
                            auto fy = float(y) / float(previewBiomeImg.height);
                            auto height = heightMapImage->SampleBilinear(fx, fy).r;
                            previewHeightImg.SetData(x, y, 0, height);
                        }
                    }

                    newPreviewHeightMap.SetData(previewHeightImg.GetData());
                }

                Common::NoiseGenerator::GeneratePerlinNoise2D(previewMoistureImg, moistureAmplitudes,
                    (uint32_t)moistureSeed);

                newPreviewMoistureMap.SetData(previewMoistureImg.GetData());

                for (int32_t y = 0; y < previewBiomeImg.height; y++) {
                    for (int32_t x = 0; x < previewBiomeImg.width; x++) {
                        auto fx = float(x) / float(previewBiomeImg.width);
                        auto fy = float(y) / float(previewBiomeImg.height);
                        auto biome = GetBiome(fx, fy, biomes, previewHeightImg,
                            previewMoistureImg, height);

                        std::pair<ResourceHandle<Material>, vec3> pair;
                        if (biome.materialIdx < 0) {
                            // Try to use the default material
                            for (auto mat : materials) {
                                if (mat.first == selectedMaterial) {
                                    pair = mat;
                                    break;
                                }
                            }
                        }
                        else {
                            pair = materials[biome.materialIdx];
                        }

                        previewBiomeImg.SetData(x, y, 0, (uint8_t)(255.0f * pair.second.r));
                        previewBiomeImg.SetData(x, y, 1, (uint8_t)(255.0f * pair.second.g));
                        previewBiomeImg.SetData(x, y, 2, (uint8_t)(255.0f * pair.second.b));
                        previewBiomeImg.SetData(x, y, 3, 255);
                    }
                }

                newPreviewBiomeMap.SetData(previewBiomeImg.GetData());
            });

    }

    std::vector<TerrainGenerator::ElevationBiome> TerrainGenerator::SortBiomes() {

        std::vector<ElevationBiome> biomes;
        std::vector<ElevationBiome> lessBiomes;
        std::vector<ElevationBiome> largerBiomes;

        for (auto& biome : elevationBiomes) {
            if (biome.less) {
                lessBiomes.push_back(biome);
            }
            else {
                largerBiomes.push_back(biome);
            }
        }

        std::sort(lessBiomes.begin(), lessBiomes.end(),
            [=](const ElevationBiome& biome1, const ElevationBiome& biome2) -> bool {

                return biome1.elevation < biome2.elevation;

            });

        std::sort(largerBiomes.begin(), largerBiomes.end(),
            [=](const ElevationBiome& biome1, const ElevationBiome& biome2) -> bool {

                return biome1.elevation > biome2.elevation;

            });

        for (auto& biome : lessBiomes) {
            biomes.push_back(biome);
        }

        for (auto& biome : largerBiomes) {
            biomes.push_back(biome);
        }

        for (auto& biome : biomes) {
            std::sort(biome.moistureBiomes.begin(), biome.moistureBiomes.end(),
                [=](const MoistureBiome& biome1, const MoistureBiome& biome2) -> bool {

                    return biome1.moisture < biome2.moisture;

                });
            std::sort(biome.slopeBiomes.begin(), biome.slopeBiomes.end(),
                [=](const SlopeBiome& biome1, const SlopeBiome& biome2) -> bool {

                    return biome1.slope < biome2.slope;

                });
        }

        return biomes;

    }

    bool TerrainGenerator::IsBiomeValid(const Biome& biome) const {

        if (biome.materialIdx < 0)
            return false;

        return materials[biome.materialIdx].first.IsLoaded();

    }

}