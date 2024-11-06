#include "TerrainGenerator.h"

#include <imgui.h>
#include <ImguiExtension/UiElements.h>
#include <tools/TerrainTool.h>
#include <common/NoiseGenerator.h>
#include <Notifications.h>
#include <Singletons.h>
#include <algorithm>

namespace Atlas::Editor {

    using namespace ImguiExtension;

    TerrainGenerator::TerrainGenerator() {

        previewHeightImg = CreateRef<Common::Image<uint16_t>>(128, 128, 1);
        previewMoistureImg = CreateRef<Common::Image<uint16_t>>(128, 128, 1);
        previewBiomeImg = CreateRef<Common::Image<uint8_t>>(128, 128, 4);

        previewHeightMap = Texture::Texture2D(128, 128, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        previewMoistureMap = Texture::Texture2D(128, 128, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        previewBiomeMap = Texture::Texture2D(128, 128, VK_FORMAT_R8G8B8A8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        newPreviewHeightMap = Texture::Texture2D(128, 128, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        newPreviewMoistureMap = Texture::Texture2D(128, 128, VK_FORMAT_R16_SNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        newPreviewBiomeMap = Texture::Texture2D(128, 128, VK_FORMAT_R8G8B8A8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        // Set this first since it will be swapped out
        newPreviewHeightMap.SetData(previewHeightImg->GetData());
        newPreviewMoistureMap.SetData(previewMoistureImg->GetData());
        newPreviewBiomeMap.SetData(previewBiomeImg->GetData());

        name.resize(500);

        auto amplitude = 1.0f;

        for (uint8_t i = 0; i < 8; i++) {
            heightAmplitudes.push_back(amplitude);
            moistureAmplitudes.push_back(amplitude);
            amplitude /= 2.0f;
        }

    }

    void TerrainGenerator::Update() {


    }

    void TerrainGenerator::Render() {

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

        ImGui::InputTextWithHint("Name", "Type name", (char*)name.data(), 500);

        ImGui::Separator();
        ImGui::Text("Heightmap");

        ImGui::RadioButton("Generate new", &loadFromFile, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Load file", &loadFromFile, 1);

        UIElements::TextureView(imguiWrapper, &previewHeightMap, width / 2.0f);
        ImGui::SetItemTooltip("Higher octaves have a higher frequency, which might not be visible in the preview\n \
                    Use the sliders to change the strength of an octave");

        if (!loadFromFile) {
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

            switch (resolutionSelection) {
            case 0: heightMapResolution = 512; break;
            case 1: heightMapResolution = 1024; break;
            case 2: heightMapResolution = 2048; break;
            case 3: heightMapResolution = 4096; break;
            case 4: heightMapResolution = 8192; break;
            }

        }
        else {
            bool resourceChanged = false;
            heightMap = textureSelectionPanel.Render(heightMap, resourceChanged);

            if (heightMap.IsLoaded() && resourceChanged) {
                heightMapImage = Loader::ImageLoader::LoadImage<uint16_t>(heightMap.GetResource()->path, false, 1);
            }
        }

        ImGui::Separator();

        ImGui::Text("General settings");

        ImGui::SliderInt("Number of LODs", &LoDCount, 1, 8);
        ImGui::SliderFloat("Resolution", &resolution, 0.25f, 2.0f);
        ImGui::SliderFloat("Height", &height, 1.0f, 1000.0f, "%.3f");

        std::string buttonText = "Advanced";

        if (advanced)
            buttonText = "Standard";

        if (ImGui::Button(buttonText.c_str(), ImVec2(width, 0.0f))) {
            advanced = !advanced;
        }

        ImGui::Separator();

        if (!advanced) {
            auto materials = ResourceManager<Material>::GetResources();
            std::vector<std::string> names;
            std::vector<const char*> pointer;

            for (auto mat : materials) {
                names.push_back(mat->name);
            }

            for (auto& name : names) {
                pointer.push_back(name.c_str());
            }
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

            int32_t matCount = 0;
            int32_t loopCount = 0;
            for (auto& [material, color] : materials) {
                auto name = material.IsValid() ? material.GetResource()->GetFileName() : "No material " + std::to_string(loopCount);

                auto treeNodeSize = region.x - (material.IsValid() ? deleteButtonSize.x + 2.0f * 8.0f : 0.0f);
                ImGui::SetNextItemWidth(treeNodeSize);
                bool open = ImGui::TreeNode(name.c_str());
                ImGui::SameLine();

                auto& deleteIcon = Singletons::icons->Get(IconType::Delete);
                auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&deleteIcon);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                    material.Reset();
                }
                ImGui::PopStyleColor();

                if (open) {
                    material = materialSelectionPanel.Render(material);
                    ImGui::ColorEdit3(("Biome color##" + std::to_string(matCount++)).c_str(), &color[0]);
                    ImGui::TreePop();
                }
                loopCount++;
            }

            if (ImGui::Button("Add material")) {
                materials.push_back({ ResourceHandle<Material>(), vec3(0.0f) });
            }

            ImGui::Separator();
            ImGui::Text("Biomes");

            UIElements::TextureView(imguiWrapper, &previewBiomeMap, width / 2.0f);
            ImGui::SetItemTooltip("Add materials to get started with the biome editing.\n"
                "Elevation biomes change the material based on the elevation of the height map.\n"
                "Moisture biomes change the material within an elevation biome based on the moisture map.");
            
            auto mats = materials;
            std::vector<std::string> names;
            std::vector<const char*> pointer;

            for (auto& [material, color] : mats) {
                auto name = material.IsValid() ? material.GetResource()->GetFileName() : "No material " + std::to_string(loopCount);
                names.push_back(name);
            }

            for (auto& name : names) {
                pointer.push_back(name.c_str());
            }

            ImGui::Combo("Default material", &materialSelection,
                pointer.data(), pointer.size());
            if ((size_t)materialSelection < materials.size()) {
                selectedMaterial = materials[materialSelection].first;
            }

            int32_t eleBiomeCount = 0;
            int32_t moiBiomeCount = 0;
            int32_t sloBiomeCount = 0;
            int32_t totalCount = 0;
            for (auto& eleBiome : elevationBiomes) {
                if (ImGui::TreeNode(("Elevation biome " + std::to_string(eleBiomeCount++)).c_str())) {
                    ImGui::SliderFloat("Elevation", &eleBiome.elevation, 0.0f, 1.0f);
                    ImGui::RadioButton("Larger", &eleBiome.less, 0);
                    ImGui::SameLine();
                    UIElements::Tooltip("Everything larger than the biome elevation will be painted with the biomes materials");
                    ImGui::SameLine();
                    ImGui::RadioButton("Less", &eleBiome.less, 1);
                    ImGui::SameLine();
                    UIElements::Tooltip("Everything smaller than the biome elevation will be painted with the biomes materials");
                    if (eleBiome.selection >= 0) {
                        ImGui::Combo(("Material##" + std::to_string(totalCount++)).c_str(), &eleBiome.selection,
                            pointer.data(), pointer.size());
                        if ((size_t)eleBiome.selection < materials.size()) {
                            eleBiome.material = materials[eleBiome.selection].first;
                        }
                    }
                    else {
                        if (ImGui::Button("Add material")) {
                            eleBiome.selection = 0;
                        }
                    }
                    if (ImGui::TreeNode("Moisture biomes")) {
                        for (auto& moiBiome : eleBiome.moistureBiomes) {
                            if (ImGui::TreeNode(("Moisture biome " + std::to_string(moiBiomeCount++)).c_str())) {
                                ImGui::SliderFloat("Moisture", &moiBiome.moisture, 0.0f, 1.0f);
                                ImGui::Combo(("Material##" + std::to_string(totalCount++)).c_str(), &moiBiome.selection,
                                    pointer.data(), pointer.size());
                                if ((size_t)moiBiome.selection < materials.size()) {
                                    moiBiome.material = materials[moiBiome.selection].first;
                                }
                                ImGui::TreePop();
                            }
                        }
                        if (ImGui::Button("Add moisture biome")) {
                            MoistureBiome biome;
                            biome.selection = 0;
                            biome.moisture = 0.1f;
                            eleBiome.moistureBiomes.push_back(biome);
                        }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Slope biomes")) {
                        for (auto& sloBiome : eleBiome.slopeBiomes) {
                            if (ImGui::TreeNode(("Slope biome " + std::to_string(sloBiomeCount++)).c_str())) {
                                ImGui::SliderFloat("Slope", &sloBiome.slope, 0.0f, 1.0f);
                                ImGui::Combo(("Material##" + std::to_string(totalCount++)).c_str(), &sloBiome.selection,
                                    pointer.data(), pointer.size());
                                if ((size_t)sloBiome.selection < materials.size()) {
                                    sloBiome.material = materials[sloBiome.selection].first;
                                }
                                ImGui::TreePop();
                            }
                        }
                        if (ImGui::Button("Add slope biome")) {
                            SlopeBiome biome;
                            biome.selection = 0;
                            biome.slope = 0.5f;
                            eleBiome.slopeBiomes.push_back(biome);
                        }
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }

            if (ImGui::Button("Add elevation biome")) {
                ElevationBiome biome;
                biome.elevation = 0.1f;
                biome.selection = -1;
                biome.less = 0;
                elevationBiomes.push_back(biome);
            }

            if (ImGui::Button("Sort biomes")) {
                elevationBiomes = SortBiomes();
            }

            ImGui::SetItemTooltip("Sorts the biome based on a biome function. After sorting, all\n"
                "biomes get evaluated from top to bottom.");                       

        }

        if (ImGui::Button("Generate", ImVec2(width, 0.0f))) {

            if (name[0] == '\0') {
                Notifications::Push({"Terrain name is missing!", vec3(1.0f, 0.0f, 0.0f)});
            }
            else if (!materials.size() && advanced) {
                Notifications::Push({"At least one material is required!", vec3(1.0f, 0.0f, 0.0f)});
            }
            else if (!selectedMaterial.IsLoaded() && !advanced) {
                Notifications::Push({"At least one material is required!", vec3(1.0f, 0.0f, 0.0f)});
            }
            else if (loadFromFile && !heightMapImage->HasData()) {
                Notifications::Push({"No heightmap loaded!", vec3(1.0f, 0.0f, 0.0f)});
            }
            else {
                successful = true;
            }

        }

        ImGui::PopID();

    }

    void TerrainGenerator::Clear() {

        name.clear();
        name.resize(500);
        heightMapResolution = 0;
        elevationBiomes.clear();
        materials.clear();
        selectedMaterial.Reset();
        materialSelection = 0;
        advanced = false;
        successful = false;

    }

    Ref<Terrain::Terrain> TerrainGenerator::GetTerrain() {

        if (!successful)
            return nullptr;

        Ref<Common::Image<uint16_t>> heightImage, moistureImage;

        if (!loadFromFile) {
            heightImage = CreateRef<Common::Image<uint16_t>>(heightMapResolution,
                heightMapResolution, 1);
            Common::NoiseGenerator::GeneratePerlinNoise2D(*heightImage, heightAmplitudes,
                (uint32_t)heightSeed, heightExp);
        }
        else {
            heightImage = heightMapImage;
        }

        Ref<Terrain::Terrain> terrain = nullptr;

        if (!advanced) {
            terrain = Tools::TerrainTool::GenerateTerrain(*heightImage, 1,
                LoDCount, 8, resolution, height, selectedMaterial);
        }
        else {
            std::vector<ResourceHandle<Material>> mats;
            Common::Image<uint8_t> splatImage(heightMapResolution,
                heightMapResolution, 1);

            moistureImage = CreateRef<Common::Image<uint16_t>>(heightMapResolution,
                heightMapResolution, 1);
            Common::NoiseGenerator::GeneratePerlinNoise2D(*moistureImage, moistureAmplitudes,
                (uint32_t)moistureSeed);

            for (auto mat : materials) {
                mats.push_back(mat.first);
            }

            auto biomes = SortBiomes();

            auto scale = height * (float)splatImage.width / (float)previewHeightImg->width;

            for (int32_t y = 0; y < splatImage.height; y++) {
                for (int32_t x = 0; x < splatImage.width; x++) {
                    auto fx = (float)x / (float)splatImage.width;
                    auto fy = (float)y / (float)splatImage.height;
                    auto pair = Biome(fx, fy, biomes, *heightImage,
                        *moistureImage, scale);
                    auto mat = pair.first;
                    uint8_t index = 0;

                    for (auto material : mats) {
                        if (material == mat)
                            break;
                        index++;
                    }
                    splatImage.SetData(x, y, 0, index);
                }
            }

            auto ref = CreateRef(splatImage);
            Loader::ImageLoader::SaveImage(ref, "Splat.png");

            terrain = Tools::TerrainTool::GenerateTerrain(*heightImage, splatImage, 1,
                LoDCount, 8, resolution, height, mats);

        }

        terrain->filename = name;

        successful = false;

        return terrain;

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
            heightExp = heightExp, heightMapImage = heightMapImage, this](JobData&) mutable {

                if (!heightMapImage) {
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
                        auto pair = Biome(fx, fy, biomes, previewHeightImg,
                            previewMoistureImg, height);
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
            [=](ElevationBiome& biome1, ElevationBiome& biome2) -> bool {

                return biome1.elevation < biome2.elevation;

            });

        std::sort(largerBiomes.begin(), largerBiomes.end(),
            [=](ElevationBiome& biome1, ElevationBiome& biome2) -> bool {

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
                [=](MoistureBiome& biome1, MoistureBiome& biome2) -> bool {

                    return biome1.moisture < biome2.moisture;

                });
            std::sort(biome.slopeBiomes.begin(), biome.slopeBiomes.end(),
                [=](SlopeBiome& biome1, SlopeBiome& biome2) -> bool {

                    return biome1.slope < biome2.slope;

                });
        }

        return biomes;

    }

    std::pair<ResourceHandle<Material>, vec3> TerrainGenerator::Biome(float x, float y, std::vector<ElevationBiome>& biomes,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, float scale) {

        ResourceHandle<Material> material;

        auto xTex = 1.0f / (float)heightImg.width;
        auto yTex = 1.0f / (float)heightImg.height;
        float heightL = (float)heightImg.SampleBilinear(x - xTex, y).r * scale / 65535.0f;
        float heightR = (float)heightImg.SampleBilinear(x + xTex, y).r * scale / 65535.0f;
        float heightD = (float)heightImg.SampleBilinear(x, y - yTex).r * scale / 65535.0f;
        float heightU = (float)heightImg.SampleBilinear(x, y + yTex).r * scale / 65535.0f;

        auto normal = glm::normalize(glm::vec3(heightL - heightR, 1.0f,
            heightD - heightU));

        auto e = (float)heightImg.SampleBilinear(x, y).r / 65535.0f;
        auto m = (float)moistureImg.SampleBilinear(x, y).r / 65535.0f;
        auto s = glm::dot(normal, vec3(0.0f, 1.0f, 0.0f));

        for (auto& eleBiome : biomes) {
            if (eleBiome.less) {
                if (e >= eleBiome.elevation) {
                    continue;
                }

                for (auto& moiBiome : eleBiome.moistureBiomes) {
                    if (m >= moiBiome.moisture) {
                        continue;
                    }
                    material = moiBiome.material;
                    if (material.IsValid())
                        break;
                }

                for (auto& sloBiome : eleBiome.slopeBiomes) {
                    if (s >= sloBiome.slope) {
                        continue;
                    }
                    material = sloBiome.material;
                    if (material.IsValid())
                        break;
                }

                if (material.IsValid())
                    break;
                material = eleBiome.material;
                if (material.IsValid())
                    break;
            }
            else {
                if (e <= eleBiome.elevation) {
                    continue;
                }

                for (auto& moiBiome : eleBiome.moistureBiomes) {
                    if (m >= moiBiome.moisture) {
                        continue;
                    }
                    material = moiBiome.material;
                    if (material.IsValid())
                        break;
                }

                for (auto& sloBiome : eleBiome.slopeBiomes) {
                    if (s >= sloBiome.slope) {
                        continue;
                    }
                    material = sloBiome.material;
                    if (material.IsValid())
                        break;
                }

                if (material.IsValid())
                    break;

                material = eleBiome.material;
                if (material.IsValid())
                    break;
            }
        }

        // Use default material
        if (!material.IsLoaded()) {
            material = selectedMaterial;
        }

        for (auto mat : materials) {
            if (mat.first == material) {
                return mat;
            }
        }

        return std::pair<ResourceHandle<Material>, vec3>(ResourceHandle<Material>{}, vec3(1.0f));

    }

}