#include "Baker.h"

#include "Evaluator.h"

#include "../common/ColorConverter.h"
#include "../common/MathHelper.h"
#include "../graphics/GraphicsDevice.h"
#include "../jobsystem/JobSystem.h"
#include "../loader/AssetLoader.h"
#include "../loader/ImageLoader.h"
#include "../pipeline/PipelineManager.h"
#include "../resource/ResourceManager.h"
#include "../streaming/StreamingManager.h"
#include "../streaming/TextureStreaming.h"

#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <system_error>

namespace Atlas::MaterialGraph {

    static const std::string bakedFolder = "baked/";

    Baker::Result Baker::Bake(const ResourceHandle<Material>& material, const ResourceHandle<Mesh::Mesh>& mesh) {

        if (!material.IsLoaded() || !material->HasGraph())
            return { .error = "The material has no graph" };
        if (!mesh.IsLoaded())
            return { .error = "The mesh isn't loaded" };
        if (!mesh->data.texCoords.ContainsData())
            return { .error = "The mesh has no texture coordinates" };

        // The handle only gives const access
        auto target = material.Get().get();
        auto& settings = target->graphBake;

        Streaming::MeshStreaming::LoadGeometry(mesh);
        Streaming::TextureStreaming::LoadFullResolution(*material.Get());

        Evaluator evaluator;
        auto shader = evaluator.PrepareBake(*material.Get());
        if (!shader.valid)
            return { .error = shader.errors.empty() ? "The graph doesn't write the surface, there is nothing to bake" :
                shader.errors.front().message };

        // An average doesn't need any details
        auto resolution = settings.mode == BakeMode::Average ? 64 : settings.resolution;

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr size_t targetCount = 5;

        std::vector<Ref<Texture::Texture2D>> targets;
        for (size_t i = 0; i < targetCount; i++) {
            targets.push_back(CreateRef<Texture::Texture2D>(resolution, resolution, format, Texture::Wrapping::ClampToEdge,
                Texture::Filtering::Nearest, false, true));
        }

        Graphics::RenderingDesc rendering = {
            .colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } },
        };
        for (size_t i = 0; i < targetCount; i++)
            rendering.colorAttachments[i] = { .image = targets[i]->image, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR };

        auto config = evaluator.GetPipelineConfig(shader, Graphics::AttachmentFormats(rendering), mesh.Get().get());
        auto pipeline = PipelineManager::GetPipeline(config);
        if (!pipeline->isComplete)
            return { .error = "The shader of the bake doesn't compile, the log has the details" };

        auto commandList = device->GetCommandList(Graphics::GraphicsQueue, true);
        commandList->BeginCommands();

        commandList->BeginRendering(rendering);
        commandList->BindPipeline(pipeline);
        evaluator.Bind(commandList);

        Evaluator::PushConstants constants = { .time = settings.time, .invertUVs = mesh->invertUVs ? 1u : 0u };
        commandList->PushConstants("constants", &constants);

        // A mesh without the material yet stands for what it will look like with it
        auto usesMaterial = std::any_of(mesh->data.subData.begin(), mesh->data.subData.end(),
            [&](const auto& subData) { return subData.material == material; });

        mesh->vertexArray.Bind(commandList);
        for (const auto& subData : mesh->data.subData) {
            if (!usesMaterial || subData.material == material)
                commandList->DrawIndexed(subData.indicesCount, 1, subData.indicesOffset);
        }

        commandList->EndRendering();

        commandList->EndCommands();
        device->FlushCommandList(commandList);

        // A single broken texel, e.g. a division by zero in a node, would otherwise end up in averages and value ranges
        auto unpack = [](float16 value) {
            auto unpacked = glm::unpackHalf1x16(uint16_t(value));
            return std::isfinite(unpacked) ? unpacked : 0.0f;
            };

        // Mapped to linear values, all channels of a texel are read at the same time later on
        const size_t texelCount = size_t(resolution) * size_t(resolution);
        std::array<std::vector<vec4>, targetCount> texels;
        for (size_t i = 0; i < targetCount; i++) {
            auto data = targets[i]->GetData<float16>();
            texels[i].resize(texelCount);
            for (size_t j = 0; j < texelCount; j++)
                texels[i][j] = vec4(unpack(data[j * 4]), unpack(data[j * 4 + 1]), unpack(data[j * 4 + 2]), unpack(data[j * 4 + 3]));
        }

        size_t coveredCount = 0;
        for (const auto& texel : texels[0])
            coveredCount += texel.a > 0.0f ? 1 : 0;

        if (!coveredCount)
            return { .error = "The mesh doesn't cover any texture coordinates from 0 to 1" };

        if (settings.mode == BakeMode::Average) {
            std::array<vec4, targetCount> sums = {};
            for (size_t i = 0; i < targetCount; i++) {
                for (const auto& texel : texels[i]) {
                    if (texel.a > 0.0f)
                        sums[i] += texel;
                }
                sums[i] /= float(coveredCount);
            }

            PrepareProperties(*material.Get());

            target->baseColor = Common::ColorConverter::ConvertLinearToSRGB(glm::max(vec3(sums[0]), vec3(0.0f)));
            target->roughness = Helper::Math::Saturate(sums[2].x);
            target->metalness = Helper::Math::Saturate(sums[2].y);
            target->ao = Helper::Math::Saturate(sums[2].z);

            auto emissive = glm::max(vec3(sums[3]), vec3(0.0f));
            auto intensity = glm::max(emissive.x, glm::max(emissive.y, emissive.z));
            target->emissiveIntensity = glm::max(intensity, 1.0f);
            target->emissiveColor = Common::ColorConverter::ConvertLinearToSRGB(emissive / target->emissiveIntensity);

            target->opacity = Helper::Math::Saturate(sums[4].x);

            settings.bakedState = GetState(*material.Get());
            return { .success = true };
        }

        // Filtering reads over the edges of the islands, so the closest covered texel continues them. All
        // properties share the same coverage and with it the source of each texel
        const int32_t size = resolution;
        std::vector<int32_t> sources(texelCount, -1);
        std::vector<int32_t> frontier;
        for (size_t i = 0; i < texelCount; i++) {
            if (texels[0][i].a > 0.0f) {
                sources[i] = int32_t(i);
                frontier.push_back(int32_t(i));
            }
        }
        const int32_t maxDilation = glm::max(size / 32, 8);
        for (int32_t pass = 0; pass < maxDilation && !frontier.empty(); pass++) {
            std::vector<int32_t> next;
            for (auto idx : frontier) {
                const int32_t x = idx % size, y = idx / size;
                const int32_t neighbours[4][2] = { { x - 1, y }, { x + 1, y }, { x, y - 1 }, { x, y + 1 } };
                for (const auto& neighbour : neighbours) {
                    if (neighbour[0] < 0 || neighbour[1] < 0 || neighbour[0] >= size || neighbour[1] >= size)
                        continue;
                    auto neighbourIdx = neighbour[1] * size + neighbour[0];
                    if (sources[neighbourIdx] < 0) {
                        sources[neighbourIdx] = sources[idx];
                        next.push_back(neighbourIdx);
                    }
                }
            }
            frontier = std::move(next);
        }

        struct Map {
            const char* name;
            size_t target;
            int32_t channel;
            int32_t channels;
            ResourceHandle<Texture::Texture2D>* handle;
            float scale = 1.0f;
            float bias = 0.0f;
        };

        // A failed write leaves the material as it was
        auto previousProperties = GetProperties(*target);
        auto previousUnbaked = settings.unbaked;
        std::vector<ResourceHandle<Texture::Texture2D>> previousMaps;
        for (const auto& map : GetBakedMaps(*target))
            previousMaps.push_back(*map.handle);

        PrepareProperties(*material.Get());

        std::vector<Map> maps;

        auto folder = std::filesystem::path(material.GetResource()->path).parent_path().generic_string();
        if (!folder.empty())
            folder += "/";
        folder += bakedFolder;
        // A folder which can't be created shows up when writing the files
        std::error_code folderError;
        std::filesystem::create_directories(Loader::AssetLoader::GetFullPath(folder), folderError);

        auto stem = std::filesystem::path(material.GetResource()->path).stem().string();

        // A property which doesn't vary only needs its value
        auto getRange = [&](size_t targetIdx, int32_t channels, int32_t channel, vec3& min, vec3& max) {
            min = vec3(std::numeric_limits<float>::max());
            max = vec3(-std::numeric_limits<float>::max());
            for (const auto& texel : texels[targetIdx]) {
                if (texel.a <= 0.0f)
                    continue;
                for (int32_t i = 0; i < channels; i++) {
                    auto value = texel[channel + i];
                    min[i] = glm::min(min[i], value);
                    max[i] = glm::max(max[i], value);
                }
            }
            };

        auto varies = [](vec3 min, vec3 max) { return glm::any(glm::greaterThan(max - min, vec3(1.0f / 255.0f))); };

        vec3 min, max;

        getRange(0, 3, 0, min, max);
        if (varies(min, max)) {
            maps.push_back({ "baseColor", 0, 0, 3, std::addressof(target->baseColorMap) });
            target->baseColor = vec3(1.0f);
        }
        else {
            target->baseColor = Common::ColorConverter::ConvertLinearToSRGB(glm::max(min, vec3(0.0f)));
        }

        getRange(1, 3, 0, min, max);
        if (varies(min, max)) {
            maps.push_back({ "normal", 1, 0, 3, std::addressof(target->normalMap), 0.5f, 0.5f });
            target->normalScale = 1.0f;
        }

        struct ScalarProperty {
            const char* name;
            size_t target;
            int32_t channel;
            ResourceHandle<Texture::Texture2D>* map;
            float* value;
        };
        const ScalarProperty scalarProperties[] = {
            { "roughness", 2, 0, std::addressof(target->roughnessMap), std::addressof(target->roughness) },
            { "metalness", 2, 1, std::addressof(target->metalnessMap), std::addressof(target->metalness) },
            { "ao", 2, 2, std::addressof(target->aoMap), std::addressof(target->ao) },
            { "opacity", 4, 0, std::addressof(target->opacityMap), std::addressof(target->opacity) }
        };

        for (const auto& property : scalarProperties) {
            getRange(property.target, 1, property.channel, min, max);
            auto hasMap = varies(min, max);
            if (hasMap)
                maps.push_back({ property.name, property.target, property.channel, 1, property.map });
            *property.value = hasMap ? 1.0f : Helper::Math::Saturate(min.x);
        }

        getRange(3, 3, 0, min, max);
        target->emissiveIntensity = glm::max(glm::max(max.x, glm::max(max.y, max.z)), 1.0f);
        if (varies(min, max)) {
            maps.push_back({ "emissive", 3, 0, 3, std::addressof(target->emissiveMap), 1.0f / target->emissiveIntensity });
            target->emissiveColor = vec3(1.0f);
        }
        else {
            auto emissive = glm::max(min, vec3(0.0f));
            target->emissiveColor = Common::ColorConverter::ConvertLinearToSRGB(emissive / target->emissiveIntensity);
        }

        // Writing the files takes the longest, each can do that on its own
        std::vector<std::string> paths(maps.size());
        std::vector<uint8_t> written(maps.size(), 0);
        JobGroup group { "Material graph bake", JobPriority::High };
        for (size_t i = 0; i < maps.size(); i++) {
            paths[i] = folder + stem + "_" + maps[i].name + ".png";
            JobSystem::Execute(group, [&, i](JobData&) {
                const auto& map = maps[i];
                const auto& mapTexels = texels[map.target];

                auto image = CreateRef<Common::Image<uint8_t>>(size, size, map.channels);
                auto& data = image->GetData();
                for (size_t texel = 0; texel < texelCount; texel++) {
                    auto source = sources[texel];
                    for (int32_t channel = 0; channel < map.channels; channel++) {
                        auto value = source >= 0 ? mapTexels[source][map.channel + channel] : (map.target == 1 && channel == 2 ? 1.0f : 0.0f);
                        data[texel * map.channels + channel] = uint8_t(Helper::Math::Saturate(value * map.scale + map.bias) * 255.0f + 0.5f);
                    }
                }

                written[i] = Loader::ImageLoader::SaveImage(image, paths[i]) ? 1 : 0;
                });
        }
        JobSystem::Wait(group);

        auto failedWrite = std::find(written.begin(), written.end(), uint8_t(0));
        if (failedWrite != written.end()) {
            SetProperties(*target, previousProperties);
            settings.unbaked = previousUnbaked;
            auto bakedMaps = GetBakedMaps(*target);
            for (size_t i = 0; i < bakedMaps.size(); i++)
                *bakedMaps[i].handle = previousMaps[i];

            auto index = std::distance(written.begin(), failedWrite);
            return { .error = "Couldn't write " + paths[index] + ", the material stays as it was" };
        }

        for (size_t i = 0; i < maps.size(); i++) {
            // A bake before already loaded a texture with the same name, which has to show the new file
            auto texture = Streaming::TextureStreaming::LoadTexture(paths[i], false, Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
            auto existing = ResourceManager<Texture::Texture2D>::GetResource(paths[i]);
            if (existing.IsValid()) {
                existing.GetResource()->Swap(texture);
                *maps[i].handle = existing;
            }
            else {
                *maps[i].handle = ResourceManager<Texture::Texture2D>::AddResource(paths[i], ResourceOrigin::User, texture);
            }
        }

        settings.bakedState = GetState(*material.Get());

        return { .success = true, .maps = int32_t(maps.size()) };

    }

    void Baker::Clear(Material& material) {

        auto& settings = material.graphBake;
        if (settings.bakedState == 0)
            return;

        const auto& unbaked = settings.unbaked;
        SetProperties(material, unbaked);

        for (const auto& map : GetBakedMaps(material)) {
            auto iter = unbaked.maps.find(map.name);
            *map.handle = iter != unbaked.maps.end() ? ResourceManager<Texture::Texture2D>::GetOrLoadResourceWithLoaderAsync(iter->second,
                Streaming::TextureStreaming::LoadTexture, false, Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic) :
                ResourceHandle<Texture::Texture2D>();
        }

        settings.unbaked = {};
        settings.bakedState = 0;

    }

    Hash Baker::GetState(const Material& material) {

        Hash hash = 0;

        if (!material.HasGraph())
            return hash;

        auto compiled = material.graph->GetCompiled();
        if (!compiled)
            return hash;

        HashCombine(hash, compiled->hash);
        HashCombine(hash, int32_t(material.graphBake.mode));
        HashCombine(hash, material.graphBake.resolution);
        HashCombine(hash, material.graphBake.time);
        HashCombine(hash, material.graphBake.meshPath);

        for (const auto& parameter : compiled->parameters) {
            if (parameter.type == ValueType::Texture) {
                const auto& texture = material.GetGraphTexture(parameter);
                if (texture.IsValid())
                    HashCombine(hash, texture.GetResource()->path);
                continue;
            }

            auto value = material.GetGraphValue(parameter);
            HashCombine(hash, value.x);
            HashCombine(hash, value.y);
            HashCombine(hash, value.z);
            HashCombine(hash, value.w);
        }

        // A value of 0 means that it wasn't baked yet
        return hash ? hash : 1;

    }

    std::array<Baker::BakedMap, 7> Baker::GetBakedMaps(Material& material) {

        return { {
            { "baseColor", std::addressof(material.baseColorMap) },
            { "normal", std::addressof(material.normalMap) },
            { "roughness", std::addressof(material.roughnessMap) },
            { "metalness", std::addressof(material.metalnessMap) },
            { "ao", std::addressof(material.aoMap) },
            { "emissive", std::addressof(material.emissiveMap) },
            { "opacity", std::addressof(material.opacityMap) }
        } };

    }

    void Baker::PrepareProperties(Material& material) {

        auto& settings = material.graphBake;
        auto isFirstBake = settings.bakedState == 0;

        if (isFirstBake)
            settings.unbaked = GetProperties(material);

        for (const auto& map : GetBakedMaps(material)) {
            if (isFirstBake && map.handle->IsValid() && !map.handle->IsGenerated())
                settings.unbaked.maps[map.name] = map.handle->GetResource()->path;
            *map.handle = ResourceHandle<Texture::Texture2D>();
        }

    }

    UnbakedProperties Baker::GetProperties(const Material& material) {

        return {
            .baseColor = material.baseColor,
            .emissiveColor = material.emissiveColor,
            .emissiveIntensity = material.emissiveIntensity,
            .opacity = material.opacity,
            .roughness = material.roughness,
            .metalness = material.metalness,
            .ao = material.ao,
            .normalScale = material.normalScale
        };

    }

    void Baker::SetProperties(Material& material, const UnbakedProperties& properties) {

        material.baseColor = properties.baseColor;
        material.emissiveColor = properties.emissiveColor;
        material.emissiveIntensity = properties.emissiveIntensity;
        material.opacity = properties.opacity;
        material.roughness = properties.roughness;
        material.metalness = properties.metalness;
        material.ao = properties.ao;
        material.normalScale = properties.normalScale;

    }

}
