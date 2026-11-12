#include "ContentPreviews.h"
#include "DataCreator.h"
#include "FileImporter.h"
#include "Singletons.h"

#include "Clock.h"
#include "Serializer.h"
#include "graphics/GraphicsDevice.h"
#include "loader/AssetLoader.h"
#include "loader/KtxLoader.h"
#include "loader/MaterialGraphLoader.h"
#include "loader/MaterialLoader.h"
#include "physics/PhysicsWorld.h"
#include "scene/components/AnimationComponent.h"
#include "scene/components/AudioComponent.h"
#include "scene/components/AudioVolumeComponent.h"
#include "scene/components/LuaScriptComponent.h"
#include "scene/prefabs/Group.h"
#include "scene/prefabs/MeshInstance.h"

#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_truetype.h>
#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <set>

namespace Atlas::Editor {

    using namespace Scene::Components;

    // Changes of how previews look have to be part of the cache key
    static constexpr int32_t previewFormatVersion = 3;

    static float SrgbToLinear(float value) {

        value = glm::clamp(value, 0.0f, 1.0f);
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);

    }

    static const std::array<float, 256>& GetSrgbToLinearTable() {

        static const auto table = []() {
            std::array<float, 256> values;
            for (int32_t i = 0; i < 256; i++)
                values[i] = SrgbToLinear(float(i) / 255.0f);
            return values;
            }();
        return table;

    }

    static uint8_t LinearToSrgb(float value) {

        // A table instead of a power per channel, the encoding runs for every pixel of every mip level
        static const auto table = []() {
            std::array<uint8_t, 4096> values;
            for (size_t i = 0; i < values.size(); i++) {
                auto linear = float(i) / float(values.size() - 1);
                auto encoded = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                values[i] = uint8_t(encoded * 255.0f + 0.5f);
            }
            return values;
            }();

        auto idx = size_t(glm::clamp(value, 0.0f, 1.0f) * float(table.size() - 1) + 0.5f);
        return table[idx];

    }

    static int64_t GetLastWriteTime(const std::string& assetPath) {

        std::error_code error;
        auto writeTime = std::filesystem::last_write_time(Loader::AssetLoader::GetFullPath(assetPath), error);
        return error ? int64_t(0) : int64_t(writeTime.time_since_epoch().count());

    }

    // Fits large images into a slot, small ones are scaled up by whole factors such that pixel art stays sharp
    static ivec2 GetPreviewSize(int32_t width, int32_t height, int32_t maxSize) {

        auto largest = glm::max(width, height);
        if (largest <= maxSize) {
            auto factor = maxSize / largest;
            return ivec2(width, height) * factor;
        }

        auto scale = float(maxSize) / float(largest);
        return glm::max(ivec2(glm::round(vec2(width, height) * scale)), ivec2(1));

    }

    // Linear colors with premultiplied alpha from fetch(x, y), averaged over the area of each target pixel
    template<class Fetch>
    static std::vector<vec4> Resample(int32_t width, int32_t height, ivec2 size, Fetch fetch) {

        std::vector<vec4> pixels(size_t(size.x) * size_t(size.y), vec4(0.0f));

        if (size.x >= width && size.y >= height) {
            for (int32_t y = 0; y < size.y; y++)
                for (int32_t x = 0; x < size.x; x++)
                    pixels[size_t(y) * size.x + x] = fetch(int32_t(int64_t(x) * width / size.x), int32_t(int64_t(y) * height / size.y));
            return pixels;
        }

        std::vector<float> counts(pixels.size(), 0.0f);
        for (int32_t y = 0; y < height; y++) {
            auto targetY = int32_t(int64_t(y) * size.y / height);
            for (int32_t x = 0; x < width; x++) {
                auto idx = size_t(targetY) * size.x + size_t(int64_t(x) * size.x / width);
                pixels[idx] += fetch(x, y);
                counts[idx] += 1.0f;
            }
        }

        for (size_t i = 0; i < pixels.size(); i++)
            pixels[i] /= glm::max(counts[i], 1.0f);

        return pixels;

    }

    // Linear colors with premultiplied alpha to sRGB encoded colors with straight alpha
    static void Encode(const std::vector<vec4>& pixels, std::vector<uint8_t>& data) {

        data.resize(pixels.size() * 4);
        for (size_t i = 0; i < pixels.size(); i++) {
            auto pixel = pixels[i];
            auto color = pixel.a > 0.0f ? vec3(pixel) / pixel.a : vec3(0.0f);
            data[i * 4 + 0] = LinearToSrgb(color.r);
            data[i * 4 + 1] = LinearToSrgb(color.g);
            data[i * 4 + 2] = LinearToSrgb(color.b);
            data[i * 4 + 3] = uint8_t(glm::clamp(pixel.a, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

    }

    static vec4 Decode(const uint8_t* pixel) {

        const auto& table = GetSrgbToLinearTable();
        auto alpha = float(pixel[3]) / 255.0f;
        return vec4(table[pixel[0]] * alpha, table[pixel[1]] * alpha, table[pixel[2]] * alpha, alpha);

    }

    ContentPreviews::PreviewAtlas::PreviewAtlas(int32_t size, uint32_t mipLevels) {

        // The interface is drawn without color conversion, the atlas holds the colors as they are shown
        format = VK_FORMAT_R8G8B8A8_UNORM;

        Reallocate(Graphics::ImageType::Image2D, size, size, 1, ::Atlas::Texture::Filtering::MipMapLinear,
            ::Atlas::Texture::Wrapping::ClampToEdge, false, false, mipLevels);
        RecreateSampler(::Atlas::Texture::Filtering::MipMapLinear, ::Atlas::Texture::Wrapping::ClampToEdge);

    }

    ContentPreviews::~ContentPreviews() {

        JobSystem::Wait(jobGroup);
        renderJob.reset();

    }

    bool ContentPreviews::IsSupported(ContentType type) {

        switch (type) {
        case ContentType::Texture:
        case ContentType::EnvironmentTexture:
        case ContentType::Font:
        case ContentType::Audio:
        case ContentType::Mesh:
        case ContentType::Material:
        case ContentType::MaterialGraph:
        case ContentType::Prefab: return true;
        default: return false;
        }

    }

    bool ContentPreviews::IsRendered(ContentType type) {

        return type == ContentType::Mesh || type == ContentType::Material ||
            type == ContentType::MaterialGraph || type == ContentType::Prefab;

    }

    bool ContentPreviews::Get(const std::string& assetPath, ContentType type, uintmax_t size, int64_t lastWriteTime, Preview& preview) {

        if (!IsSupported(type))
            return false;

        Hash version = 0;
        HashCombine(version, assetPath);
        HashCombine(version, int32_t(type));
        HashCombine(version, uint64_t(size));
        HashCombine(version, lastWriteTime);
        HashCombine(version, slotSize);
        HashCombine(version, previewFormatVersion);

        auto [iter, inserted] = entries.try_emplace(assetPath);
        auto& entry = iter->second;

        // A changed file keeps showing its last preview until the new one is there
        if (inserted || entry.version != version) {
            entry.version = version;
            entry.type = type;
            entry.state = State::Queued;
            entry.retryTime = 0.0f;
        }

        if (entry.state == State::Failed && entry.retryTime > 0.0f && Clock::Get() >= entry.retryTime) {
            entry.state = State::Queued;
            entry.retryTime = 0.0f;
        }

        if (entry.state == State::Queued && entry.lastRequestFrame != frame)
            requested.push_back(assetPath);
        entry.lastRequestFrame = frame;

        if (!entry.hasImage || !atlas)
            return false;

        auto atlasSize = vec2(float(atlas->width), float(atlas->height));
        auto position = vec2(GetSlotPosition(entry.slot));
        // Half a texel inside, such that the neighboring slots don't shine through at the edges
        auto uvMin = (position + 0.5f) / atlasSize;
        auto uvMax = (position + vec2(entry.size) - 0.5f) / atlasSize;

        preview.texture = Singletons::imguiWrapper->GetTextureId(atlas.get());
        preview.uvMin = ImVec2(uvMin.x, uvMin.y);
        preview.uvMax = ImVec2(uvMax.x, uvMax.y);
        preview.size = ImVec2(float(entry.size.x), float(entry.size.y));
        preview.tintable = type == ContentType::Font || type == ContentType::Audio;
        preview.transparent = IsRendered(type);
        preview.alpha = glm::clamp((Clock::Get() - entry.readyTime) / fadeTime, 0.0f, 1.0f);

        return true;

    }

    void ContentPreviews::Update() {

        lastUpdateTime = Clock::Get();

        if (!atlas && !requested.empty())
            Init();

        if (atlas) {
            ProcessResults();
            StartJobs();
            UpdateRendering();
            Evict();
        }

        requested.clear();
        frame++;

    }

    void ContentPreviews::ReleaseUnused() {

        auto now = Clock::Get();

        // Nothing is rendered for previews which aren't shown, the content is rendered again once it is visible
        if (renderJob && now - lastUpdateTime > hiddenTime)
            CancelRender();

        if (renderer && !renderJob && now - lastRenderTime > rendererIdleTime) {
            renderer.reset();
            renderTarget.reset();
            renderViewport.reset();
            renderTexture.Reset();
        }

    }

    void ContentPreviews::Init() {

        atlas = CreateRef<PreviewAtlas>(slotSize * slotsPerRow, mipLevels);

    }

    void ContentPreviews::StartJobs() {

        for (const auto& assetPath : requested) {
            if (runningJobs >= maxRunningJobs)
                break;

            auto iter = entries.find(assetPath);
            if (iter == entries.end() || iter->second.state != State::Queued)
                continue;

            auto& entry = iter->second;
            // The slot is reserved before anything is generated, the content shows its icon without one
            if (entry.slot < 0)
                entry.slot = AllocateSlot();
            if (entry.slot < 0)
                break;

            entry.state = State::Loading;
            runningJobs++;

            JobSystem::Execute(jobGroup, [this, assetPath, type = entry.type, version = entry.version](JobData&) {
                auto result = Generate(assetPath, type, version);
                {
                    std::scoped_lock lock(resultMutex);
                    results.push_back(std::move(result));
                }
                runningJobs--;
                });
        }

    }

    void ContentPreviews::ProcessResults() {

        std::vector<Result> finished;
        {
            std::scoped_lock lock(resultMutex);
            finished.swap(results);
        }

        std::vector<std::pair<Entry*, const Result*>> uploads;
        for (const auto& result : finished) {
            auto iter = entries.find(result.assetPath);
            if (iter == entries.end() || iter->second.version != result.version || iter->second.slot < 0)
                continue;

            auto& entry = iter->second;
            if (result.failed) {
                entry.state = State::Failed;
            }
            else if (result.needsRender) {
                entry.state = State::Rendering;
                renderQueue.push_back(result.assetPath);
            }
            else {
                uploads.emplace_back(&entry, &result);
            }
        }

        Upload(uploads);

    }

    std::vector<uint8_t> ContentPreviews::BuildSlotLevels(const Image& image) {

        std::vector<uint8_t> levels;

        // The whole slot is written, the parts next to the preview are transparent
        std::vector<vec4> level(size_t(slotSize) * slotSize, vec4(0.0f));
        for (int32_t y = 0; y < image.height; y++)
            for (int32_t x = 0; x < image.width; x++)
                level[size_t(y) * slotSize + x] = Decode(&image.data[(size_t(y) * image.width + x) * 4]);

        auto levelSize = slotSize;
        std::vector<uint8_t> data;
        for (uint32_t mipLevel = 0; mipLevel < mipLevels; mipLevel++) {
            if (mipLevel > 0) {
                auto previousSize = levelSize;
                levelSize /= 2;
                std::vector<vec4> next(size_t(levelSize) * levelSize);
                for (int32_t y = 0; y < levelSize; y++) {
                    for (int32_t x = 0; x < levelSize; x++) {
                        auto idx = size_t(2 * y) * previousSize + size_t(2 * x);
                        next[size_t(y) * levelSize + x] = 0.25f * (level[idx] + level[idx + 1] +
                            level[idx + previousSize] + level[idx + previousSize + 1]);
                    }
                }
                level = std::move(next);
            }

            Encode(level, data);
            levels.insert(levels.end(), data.begin(), data.end());
        }

        return levels;

    }

    void ContentPreviews::Upload(const std::vector<std::pair<Entry*, const Result*>>& uploads) {

        if (uploads.empty())
            return;

        std::vector<uint8_t> buffer;
        std::vector<VkBufferImageCopy> regions;

        for (const auto& [entry, result] : uploads) {
            auto position = GetSlotPosition(entry->slot);
            size_t levelOffset = 0;
            for (uint32_t mipLevel = 0; mipLevel < mipLevels; mipLevel++) {
                auto levelSize = slotSize >> mipLevel;
                auto levelBytes = size_t(levelSize) * size_t(levelSize) * 4;

                VkBufferImageCopy region = {};
                region.bufferOffset = buffer.size() + levelOffset;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = mipLevel;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = { position.x >> mipLevel, position.y >> mipLevel, 0 };
                region.imageExtent = { uint32_t(levelSize), uint32_t(levelSize), 1 };
                regions.push_back(region);

                levelOffset += levelBytes;
            }
            buffer.insert(buffer.end(), result->slotLevels.begin(), result->slotLevels.end());

            // Only new previews fade in, not the ones of changed content
            if (!entry->hasImage)
                entry->readyTime = Clock::Get();
            entry->size = ivec2(result->image.width, result->image.height);
            entry->hasImage = true;
            entry->state = State::Ready;
        }

        // Recorded with the other work of the frame instead of waiting for the device, the device keeps the
        // staging buffer until the frame is done
        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto stagingBuffer = device->CreateBuffer({
            .usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .domain = Graphics::BufferDomain::Host,
            .data = buffer.data(),
            .size = buffer.size()
            });

        auto commandList = device->GetCommandList(Graphics::GraphicsQueue);
        commandList->BeginCommands();
        commandList->ImageMemoryBarrier(atlas->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdCopyBufferToImage(commandList->commandBuffer, stagingBuffer->buffer, atlas->image->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uint32_t(regions.size()), regions.data());
        commandList->ImageMemoryBarrier(atlas->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        commandList->EndCommands();
        device->SubmitCommandList(commandList);

    }

    int32_t ContentPreviews::AllocateSlot() {

        if (!freeSlots.empty()) {
            auto slot = freeSlots.back();
            freeSlots.pop_back();
            return slot;
        }

        if (nextSlot < slotsPerRow * slotsPerRow)
            return nextSlot++;

        // The preview which wasn't shown for the longest time makes room, visible ones are kept
        Entry* leastRecent = nullptr;
        for (auto& [_, entry] : entries) {
            if (entry.slot < 0 || entry.state == State::Loading || entry.state == State::Rendering ||
                entry.lastRequestFrame >= frame - 1)
                continue;
            if (!leastRecent || entry.lastRequestFrame < leastRecent->lastRequestFrame)
                leastRecent = &entry;
        }

        if (!leastRecent)
            return -1;

        auto slot = leastRecent->slot;
        leastRecent->slot = -1;
        leastRecent->hasImage = false;
        // Generated again once it is visible, the disk cache makes this fast
        leastRecent->state = State::Queued;

        return slot;

    }

    void ContentPreviews::Evict() {

        // Content which wasn't visible for a while is forgotten, e.g. after the user browsed through a large project
        const int64_t forgetAfterFrames = 3600;

        for (auto iter = entries.begin(); iter != entries.end();) {
            auto& entry = iter->second;
            auto inFlight = entry.state == State::Loading || entry.state == State::Rendering;
            if (!inFlight && frame - entry.lastRequestFrame > forgetAfterFrames) {
                if (entry.slot >= 0)
                    freeSlots.push_back(entry.slot);
                iter = entries.erase(iter);
            }
            else {
                iter++;
            }
        }

    }

    ivec2 ContentPreviews::GetSlotPosition(int32_t slot) const {

        return ivec2(slot % slotsPerRow, slot / slotsPerRow) * slotSize;

    }

    void ContentPreviews::UpdateRendering() {

        while (!renderJob && !renderQueue.empty()) {
            auto assetPath = renderQueue.front();
            renderQueue.erase(renderQueue.begin());

            auto iter = entries.find(assetPath);
            if (iter == entries.end() || iter->second.state != State::Rendering)
                continue;

            // Content which was scrolled out of view is queued again once it is visible
            auto& entry = iter->second;
            if (entry.lastRequestFrame < frame - 1) {
                entry.state = State::Queued;
                continue;
            }

            renderJob = std::make_unique<RenderJob>();
            renderJob->assetPath = assetPath;
            renderJob->version = entry.version;
            renderJob->type = entry.type;

            if (!StartRender(*renderJob))
                FinishRender(true, Image());
        }

        if (!renderJob)
            return;

        lastRenderTime = Clock::Get();

        // Waiting for the editor doesn't count towards the timeout
        if (Singletons::blockingOperation->block)
            return;

        auto& job = *renderJob;
        job.activeTime += Clock::GetDelta();

        // The frames were submitted at the end of the last frame
        if (job.waitForCapture) {
            auto image = Capture();
            FinishRender(image.data.empty(), std::move(image));
            return;
        }

        auto timedOut = job.activeTime > renderTimeout;
        auto failedToLoad = [](const auto& handle) {
            return handle.IsValid() && handle.GetResource()->errorOnLoad;
            };

        if (failedToLoad(job.mesh) || failedToLoad(job.material) || failedToLoad(job.graph)) {
            FinishRender(true, Image());
            return;
        }

        if (!job.scene) {
            auto isLoaded = [](const auto& handle) {
                return !handle.IsValid() || handle.IsLoaded();
                };
            if (!isLoaded(job.mesh) || !isLoaded(job.material) || !isLoaded(job.graph)) {
                if (timedOut)
                    FinishRender(true, Image());
                return;
            }

            CreateRenderScene(job);
            if (!job.scene) {
                FinishRender(true, Image());
                return;
            }
        }

        // Textures of the materials are loaded in the background
        if (!job.scene->IsFullyLoaded()) {
            if (!timedOut)
                return;
            job.complete = false;
        }

        RenderFrame(job);

    }

    bool ContentPreviews::StartRender(RenderJob& job) {

        // Everything is loaded in the background, the job waits for it
        switch (job.type) {
        case ContentType::Mesh:
            job.mesh = FileImporter::ImportFile<Mesh::Mesh>(job.assetPath);
            return job.mesh.IsValid();
        case ContentType::Material:
            job.material = ResourceManager<Material>::GetOrLoadResourceWithLoaderAsync(job.assetPath,
                ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
            return job.material.IsValid();
        case ContentType::MaterialGraph:
            job.graph = ResourceManager<MaterialGraph::Graph>::GetOrLoadResourceWithLoaderAsync(job.assetPath,
                ResourceOrigin::User, Loader::MaterialGraphLoader::LoadGraph);
            return job.graph.IsValid();
        case ContentType::Prefab:
            return true;
        default:
            return false;
        }

    }

    void ContentPreviews::CreateRenderScene(RenderJob& job) {

        auto scene = CreateRef<Scene::Scene>("Content preview");
        // Like the scenes of the editor, prefabs with rigid bodies or scripts expect one. It never runs
        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        auto rootGroup = scene->CreatePrefab<Scene::Prefabs::Group>("Root");
        rootGroup.GetComponent<HierarchyComponent>().root = true;

        Scene::Entity entity;
        if (job.type == ContentType::Mesh) {
            entity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(job.mesh, false);
            // Skinned meshes need an animation component to be shown
            if (job.mesh->data.IsSkinned())
                entity.AddComponent<AnimationComponent>();
        }
        else if (job.type == ContentType::Material || job.type == ContentType::MaterialGraph) {
            // Graphs are shown with a material which only exists for the preview
            if (job.type == ContentType::MaterialGraph) {
                job.material = ResourceHandle<Material>(CreateRef<Material>());
                job.material->graph = job.graph;
            }
            // Fewer vertices than the sphere of the material editor, a small preview doesn't show displacement in detail
            auto sphere = ResourceHandle<Mesh::Mesh>(DataCreator::CreateSphere(job.material, 64, 96));
            job.mesh = sphere;
            entity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(sphere, false);
        }
        else if (job.type == ContentType::Prefab) {
            try {
                entity = Serializer::DeserializePrefab(scene, job.assetPath);
            }
            catch (const std::exception&) {
                entity = Scene::Entity();
            }
            if (!entity.IsValid())
                return;

            // A preview only shows the prefab, scripts aren't executed and sounds aren't played
            auto removeAll = [&]<class Comp>() {
                std::vector<ECS::Entity> entities;
                for (auto componentEntity : scene->entityManager.GetSubset<Comp>())
                    entities.push_back(componentEntity);
                for (auto componentEntity : entities)
                    scene->entityManager.Erase<Comp>(componentEntity);
                };
            removeAll.template operator()<LuaScriptComponent>();
            removeAll.template operator()<AudioComponent>();
            removeAll.template operator()<AudioVolumeComponent>();
        }

        job.root = entity;

        job.camera = scene->CreateEntity();
        auto& camera = job.camera.AddComponent<CameraComponent>(40.0f, 1.0f, 0.01f, 100.0f,
            vec3(0.0f), vec2(glm::pi<float>() + 0.6f, -0.55f));
        camera.isMain = true;

        job.scene = scene;

    }

    void ContentPreviews::RenderFrame(RenderJob& job) {

        // A frame which the renderer skips, e.g. while the window is minimized, doesn't count
        auto device = Graphics::GraphicsDevice::DefaultDevice;
        if (!device->swapChain->isComplete)
            return;

        auto renderSize = slotSize * renderScale;
        if (!renderer) {
            renderer = CreateRef<Renderer::MainRenderer>();
            renderer->Init(device);
            renderTarget = CreateRef<Renderer::RenderTarget>(renderSize, renderSize);
            renderViewport = CreateRef<Viewport>(0, 0, renderSize, renderSize);
            renderTexture = Texture::Texture2D(renderSize, renderSize, VK_FORMAT_R16G16B16A16_SFLOAT);
        }

        const float deltaTime = 1.0f / 60.0f;
        auto& scene = job.scene;

        if (job.renderedFrames == 0) {
            // The camera direction and the bounds of prefabs are known after an update
            scene->Update(deltaTime, job.camera);

            Volume::AABB bounds(vec3(-1.0f), vec3(1.0f));
            if (job.type == ContentType::Prefab)
                bounds = scene->GetHierarchyBounds(job.root).value_or(bounds);
            else if (job.mesh.IsLoaded())
                bounds = job.mesh->data.aabb;

            // The sphere of materials fills the preview, other content fits in with its bounding sphere
            auto center = 0.5f * (bounds.min + bounds.max);
            auto radius = glm::max(0.5f * glm::length(bounds.max - bounds.min), 0.01f);
            if (job.type == ContentType::Material || job.type == ContentType::MaterialGraph)
                radius = 1.05f;

            auto& camera = job.camera.GetComponent<CameraComponent>();
            auto distance = radius / std::sin(glm::radians(camera.fieldOfView) * 0.5f);
            camera.location = center - camera.direction * distance;
            camera.nearPlane = 0.01f * distance;
            camera.farPlane = 4.0f * (distance + radius);

            // Prefabs might bring their own light
            auto hasLight = false;
            for (auto lightEntity : scene->GetSubset<LightComponent>()) {
                hasLight = true;
                break;
            }
            if (!hasLight) {
                auto lightEntity = scene->CreateEntity();
                auto& light = lightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);
                light.properties.directional.direction = vec3(-0.5f, -1.0f, -1.0f);
                light.color = vec3(255, 236, 209) / 255.0f;
                light.intensity = 10.0f;
                light.AddDirectionalShadow(distance + 2.0f * radius, 3.0f, 1024, 0.05f, 1, 0.9f);
                light.isMain = true;
            }
        }

        scene->Update(deltaTime, job.camera);

        renderer->Update();
        renderer->RenderScene(renderViewport, renderTarget, scene, {}, &renderTexture);
        job.renderedFrames++;

        // Graphs compile their pipelines in the background, the frames only count once they are used
        auto graphPending = false;
        for (const auto& mesh : scene->GetMeshes()) {
            if (!mesh.IsLoaded())
                continue;
            for (const auto& subData : mesh->data.subData)
                graphPending |= subData.mainGraphPending;
        }
        if (graphPending) {
            if (job.activeTime <= renderTimeout)
                job.renderedFrames = 1;
            else
                job.complete = false;
        }

        if (job.renderedFrames >= renderFrames)
            job.waitForCapture = true;

    }

    ContentPreviews::Image ContentPreviews::Capture() {

        Image image;

        // The swap of the render target moved the depth of the last frame into the history
        auto depthTexture = renderTarget->GetHistoryData(Renderer::FULL_RES)->depthTexture;
        if (!depthTexture || renderTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED)
            return image;

        // Only the first read waits for the device, the second one reads from the same finished frame
        auto colorImage = renderTexture.GetImage<float>();
        std::vector<float> depth(size_t(depthTexture->width) * size_t(depthTexture->height));
        auto transferManager = Graphics::GraphicsDevice::DefaultDevice->memoryManager->transferManager;
        transferManager->RetrieveImageData(depth.data(), depthTexture->image.get(), VkOffset3D{ 0, 0, 0 },
            VkExtent3D{ uint32_t(depthTexture->width), uint32_t(depthTexture->height), 1 }, 0, 1, false);

        const auto& colors = colorImage->GetData();
        auto width = renderTexture.width;
        auto height = renderTexture.height;
        if (colors.size() < size_t(width) * size_t(height) * 4 || depth.empty())
            return image;

        auto getDepth = [&](int32_t x, int32_t y) {
            auto depthX = x * depthTexture->width / width;
            auto depthY = y * depthTexture->height / height;
            return depth[size_t(depthY) * depthTexture->width + depthX];
            };

        // The content fits into the view, so a corner shows the background
        auto background = getDepth(0, 0);

        // The post processing already encoded the colors for the display, they are averaged as linear colors
        auto size = ivec2(width, height) / renderScale;
        std::vector<vec4> pixels(size_t(size.x) * size_t(size.y), vec4(0.0f));
        for (int32_t y = 0; y < size.y; y++) {
            for (int32_t x = 0; x < size.x; x++) {
                vec4 sum(0.0f);
                for (int32_t sampleY = 0; sampleY < renderScale; sampleY++) {
                    for (int32_t sampleX = 0; sampleX < renderScale; sampleX++) {
                        auto pixelX = x * renderScale + sampleX;
                        auto pixelY = y * renderScale + sampleY;
                        if (getDepth(pixelX, pixelY) == background)
                            continue;
                        auto idx = (size_t(pixelY) * width + pixelX) * 4;
                        sum += vec4(SrgbToLinear(colors[idx]), SrgbToLinear(colors[idx + 1]), SrgbToLinear(colors[idx + 2]), 1.0f);
                    }
                }
                pixels[size_t(y) * size.x + x] = sum / float(renderScale * renderScale);
            }
        }

        // Nothing in view, e.g. an empty prefab
        auto empty = std::all_of(pixels.begin(), pixels.end(), [](const vec4& pixel) { return pixel.a == 0.0f; });
        if (empty)
            return image;

        image.width = size.x;
        image.height = size.y;
        Encode(pixels, image.data);

        return image;

    }

    void ContentPreviews::FinishRender(bool failed, Image image) {

        auto& job = *renderJob;

        auto iter = entries.find(job.assetPath);
        if (iter != entries.end() && iter->second.version == job.version) {
            if (failed) {
                // Loading or rendering might work later, e.g. after a long import
                iter->second.state = State::Failed;
                iter->second.retryTime = Clock::Get() + retryDelay;
            }
            else {
                // Incomplete previews are shown, but not cached
                auto dependencies = job.complete ? GetDependencies(job) : std::vector<std::string>();
                JobSystem::Execute(jobGroup, [this, assetPath = job.assetPath, version = job.version, complete = job.complete,
                    dependencies, image = std::move(image)](JobData&) {
                    if (complete)
                        WriteCache(version, image, dependencies);
                    auto result = Result{ .assetPath = assetPath, .version = version, .image = image };
                    result.slotLevels = BuildSlotLevels(image);
                    std::scoped_lock lock(resultMutex);
                    results.push_back(std::move(result));
                    });
            }
        }

        renderJob.reset();

    }

    void ContentPreviews::CancelRender() {

        auto iter = entries.find(renderJob->assetPath);
        if (iter != entries.end() && iter->second.version == renderJob->version)
            iter->second.state = State::Queued;

        renderJob.reset();

    }

    std::vector<std::string> ContentPreviews::GetDependencies(RenderJob& job) {

        std::set<std::string> paths;
        auto add = [&](const auto& handle) {
            if (handle.IsValid() && !handle.IsGenerated())
                paths.insert(handle.GetResource()->path);
            };

        for (const auto& mesh : job.scene->GetMeshes())
            add(mesh);

        for (const auto& material : job.scene->GetMaterials()) {
            add(material);
            add(material->graph);
            for (const auto& map : material->GetMaps())
                add(map);
            for (const auto& map : material->GetGraphTextures())
                add(map);
        }

        // The file of the preview itself is part of the version
        paths.erase(job.assetPath);

        return std::vector<std::string>(paths.begin(), paths.end());

    }

    ContentPreviews::Result ContentPreviews::Generate(const std::string& assetPath, ContentType type, Hash version) {

        Result result = { .assetPath = assetPath, .version = version };

        try {
            if (!ReadCache(version, IsRendered(type), result.image)) {
                if (IsRendered(type)) {
                    result.needsRender = true;
                    return result;
                }

                switch (type) {
                case ContentType::Texture: result.image = LoadTexture(assetPath); break;
                case ContentType::EnvironmentTexture: result.image = LoadEnvironmentTexture(assetPath); break;
                case ContentType::Font: result.image = LoadFont(assetPath); break;
                case ContentType::Audio: result.image = LoadAudio(assetPath); break;
                default: break;
                }

                if (!result.image.data.empty())
                    WriteCache(version, result.image, {});
            }
        }
        catch (const std::exception&) {
            result.image = Image();
        }

        result.failed = result.image.data.empty();
        if (!result.failed)
            result.slotLevels = BuildSlotLevels(result.image);

        return result;

    }

    std::string ContentPreviews::GetCachePath(Hash version) {

        // Not an image extension, the content discovery would find the previews otherwise
        return ".cache/previews/" + std::to_string(version) + ".preview";

    }

    void ContentPreviews::WriteCache(Hash version, const Image& image, const std::vector<std::string>& dependencies) {

        std::vector<char> fileData;
        auto write = [](void* context, void* data, int size) {
            auto fileData = static_cast<std::vector<char>*>(context);
            fileData->insert(fileData->end(), static_cast<char*>(data), static_cast<char*>(data) + size);
            };
        if (!stbi_write_png_to_func(write, &fileData, image.width, image.height, 4, image.data.data(), image.width * 4))
            return;

        // The files the preview depends on with their time of modification, a change of one renders it again
        std::string dependencyData;
        for (const auto& dependency : dependencies)
            dependencyData += dependency + "\n" + std::to_string(GetLastWriteTime(dependency)) + "\n";

        // Written aside and renamed, a preview is never read while it is written
        auto writeFile = [](const std::string& path, const char* data, size_t size) {
            auto tempPath = path + "." + std::to_string(std::random_device()());
            auto stream = Loader::AssetLoader::WriteFile(tempPath, std::ios::out | std::ios::binary);
            if (!stream.is_open())
                return;

            stream.write(data, std::streamsize(size));
            stream.close();

            std::error_code error;
            if (!stream.fail())
                std::filesystem::rename(Loader::AssetLoader::GetFullPath(tempPath), Loader::AssetLoader::GetFullPath(path), error);
            if (stream.fail() || error)
                Loader::AssetLoader::RemoveFile(tempPath);
            };

        auto cachePath = GetCachePath(version);
        writeFile(cachePath + ".dependencies", dependencyData.data(), dependencyData.size());
        writeFile(cachePath, fileData.data(), fileData.size());

    }

    bool ContentPreviews::ReadCache(Hash version, bool checkDependencies, Image& image) {

        auto cachePath = GetCachePath(version);
        if (!Loader::AssetLoader::FileExists(cachePath))
            return false;

        if (checkDependencies) {
            auto dependencyPath = cachePath + ".dependencies";
            if (!Loader::AssetLoader::FileExists(dependencyPath))
                return false;

            auto stream = Loader::AssetLoader::ReadFile(dependencyPath, std::ios::in);
            std::string path, writeTime;
            while (std::getline(stream, path) && std::getline(stream, writeTime)) {
                if (std::to_string(GetLastWriteTime(path)) != writeTime)
                    return false;
            }
        }

        auto stream = Loader::AssetLoader::ReadFile(cachePath, std::ios::in | std::ios::binary);
        if (!stream.is_open())
            return false;

        auto buffer = Loader::AssetLoader::GetFileContent(stream);
        int32_t width, height, channels;
        auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(buffer.data()), int32_t(buffer.size()),
            &width, &height, &channels, 4);
        if (!pixels)
            return false;

        auto fits = width > 0 && height > 0 && width <= slotSize && height <= slotSize;
        if (fits) {
            image.width = width;
            image.height = height;
            image.data.assign(pixels, pixels + size_t(width) * size_t(height) * 4);
        }
        stbi_image_free(pixels);

        return fits;

    }

    ContentPreviews::Image ContentPreviews::LoadTexture(const std::string& assetPath) {

        Image image;

        int32_t width = 0, height = 0;
        Ref<Common::Image<uint8_t>> ktxImage;
        stbi_uc* stbiData = nullptr;

        if (Loader::KtxLoader::IsKtxFile(assetPath)) {
            auto stream = Loader::AssetLoader::ReadFile(assetPath, std::ios::in | std::ios::binary);
            if (!stream.is_open())
                return image;
            // Only the level closest to the preview size is decoded
            ktxImage = Loader::KtxLoader::DecodeImage(Loader::AssetLoader::GetFileContent(stream), slotSize);
            if (!ktxImage)
                return image;
            width = ktxImage->width;
            height = ktxImage->height;
        }
        else {
            // Streamed from the file, only the decoded image is in memory
            int32_t channels;
            stbiData = stbi_load(Loader::AssetLoader::GetFullPath(assetPath).c_str(), &width, &height, &channels, 4);
            if (!stbiData)
                return image;
        }

        const uint8_t* pixels = ktxImage ? ktxImage->GetData().data() : stbiData;
        auto size = GetPreviewSize(width, height, slotSize);
        auto resampled = Resample(width, height, size, [&](int32_t x, int32_t y) {
            return Decode(&pixels[(size_t(y) * width + x) * 4]);
            });

        if (stbiData)
            stbi_image_free(stbiData);

        image.width = size.x;
        image.height = size.y;
        Encode(resampled, image.data);

        return image;

    }

    ContentPreviews::Image ContentPreviews::LoadEnvironmentTexture(const std::string& assetPath) {

        Image image;

        // Streamed from the file, only the decoded image is in memory
        int32_t width, height, channels;
        auto data = stbi_loadf(Loader::AssetLoader::GetFullPath(assetPath).c_str(), &width, &height, &channels, 3);
        if (!data)
            return image;

        auto size = GetPreviewSize(width, height, slotSize);
        auto resampled = Resample(width, height, size, [&](int32_t x, int32_t y) {
            auto pixel = data + (size_t(y) * width + x) * 3;
            return vec4(pixel[0], pixel[1], pixel[2], 1.0f);
            });
        stbi_image_free(data);

        // The exposure adapts to the average brightness, environments have very different ranges
        double logLuminance = 0.0;
        for (const auto& pixel : resampled)
            logLuminance += std::log(1e-4 + double(glm::dot(vec3(pixel), vec3(0.2126f, 0.7152f, 0.0722f))));
        auto averageLuminance = float(std::exp(logLuminance / double(glm::max(resampled.size(), size_t(1)))));
        auto exposure = glm::clamp(0.18f / averageLuminance, 1e-4f, 1e4f);

        for (auto& pixel : resampled) {
            // Fitted ACES curve
            auto color = vec3(pixel) * exposure * 0.6f;
            color = glm::clamp((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f), 0.0f, 1.0f);
            pixel = vec4(color, 1.0f);
        }

        image.width = size.x;
        image.height = size.y;
        Encode(resampled, image.data);

        return image;

    }

    ContentPreviews::Image ContentPreviews::LoadFont(const std::string& assetPath) {

        Image image;

        auto stream = Loader::AssetLoader::ReadFile(assetPath, std::ios::in | std::ios::binary);
        if (!stream.is_open())
            return image;
        auto buffer = Loader::AssetLoader::GetFileContent(stream);
        stream.close();

        auto fontData = reinterpret_cast<const unsigned char*>(buffer.data());
        auto offset = stbtt_GetFontOffsetForIndex(fontData, 0);
        stbtt_fontinfo font;
        if (offset < 0 || !stbtt_InitFont(&font, fontData, offset))
            return image;

        const int32_t codepoints[] = { 'A', 'a' };

        // Calls the function with the position of each glyph on the line
        auto forEachGlyph = [&](float scale, auto function) {
            float pen = 0.0f;
            for (size_t i = 0; i < std::size(codepoints); i++) {
                function(codepoints[i], int32_t(pen));

                int32_t advance, leftBearing;
                stbtt_GetCodepointHMetrics(&font, codepoints[i], &advance, &leftBearing);
                pen += float(advance) * scale;
                if (i + 1 < std::size(codepoints))
                    pen += float(stbtt_GetCodepointKernAdvance(&font, codepoints[i], codepoints[i + 1])) * scale;
            }
            };

        // The bounds of the text at a reference size, the text is then scaled to fill the preview
        auto measure = [&](float scale, ivec2& min, ivec2& max) {
            min = ivec2(std::numeric_limits<int32_t>::max());
            max = ivec2(std::numeric_limits<int32_t>::min());
            forEachGlyph(scale, [&](int32_t codepoint, int32_t pen) {
                int32_t x0, y0, x1, y1;
                stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
                min = glm::min(min, ivec2(pen + x0, y0));
                max = glm::max(max, ivec2(pen + x1, y1));
                });
            };

        ivec2 min, max;
        auto referenceScale = stbtt_ScaleForPixelHeight(&font, 100.0f);
        measure(referenceScale, min, max);
        auto referenceSize = vec2(max - min);
        if (referenceSize.x <= 0.0f || referenceSize.y <= 0.0f)
            return image;

        auto fit = glm::min(0.72f * float(slotSize) / referenceSize.x, 0.6f * float(slotSize) / referenceSize.y);
        auto scale = referenceScale * fit;
        measure(scale, min, max);

        image.width = slotSize;
        image.height = slotSize;
        image.data.assign(size_t(slotSize) * slotSize * 4, 0);

        auto origin = (ivec2(slotSize) - (max - min)) / 2 - min;
        forEachGlyph(scale, [&](int32_t codepoint, int32_t pen) {
            int32_t width, height, offsetX, offsetY;
            auto bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, codepoint, &width, &height, &offsetX, &offsetY);
            if (!bitmap)
                return;

            for (int32_t y = 0; y < height; y++) {
                for (int32_t x = 0; x < width; x++) {
                    auto targetX = origin.x + pen + offsetX + x;
                    auto targetY = origin.y + offsetY + y;
                    if (targetX < 0 || targetY < 0 || targetX >= slotSize || targetY >= slotSize)
                        continue;
                    auto idx = (size_t(targetY) * slotSize + targetX) * 4;
                    image.data[idx + 0] = 255;
                    image.data[idx + 1] = 255;
                    image.data[idx + 2] = 255;
                    image.data[idx + 3] = std::max(image.data[idx + 3], bitmap[size_t(y) * width + x]);
                }
            }
            stbtt_FreeBitmap(bitmap, nullptr);
            });

        return image;

    }

    ContentPreviews::Image ContentPreviews::LoadAudio(const std::string& assetPath) {

        Image image;

        // Streamed from the file, the file content isn't kept next to the samples
        SDL_AudioSpec spec;
        uint8_t* data = nullptr;
        uint32_t length = 0;
        if (!SDL_LoadWAV(Loader::AssetLoader::GetFullPath(assetPath).c_str(), &spec, &data, &length))
            return image;

        // One channel of floats is enough to draw the waveform
        SDL_AudioSpec floatSpec = { .format = SDL_AUDIO_F32, .channels = 1, .freq = spec.freq };
        uint8_t* converted = nullptr;
        int32_t convertedLength = 0;
        auto success = SDL_ConvertAudioSamples(&spec, data, int32_t(length), &floatSpec, &converted, &convertedLength);
        SDL_free(data);
        if (!success)
            return image;

        auto samples = reinterpret_cast<const float*>(converted);
        auto sampleCount = size_t(convertedLength) / sizeof(float);

        // Bars with the loudness of a part of the audio, the loudest one fills the height
        const int32_t barWidth = 3;
        const int32_t barSpacing = 2;
        const int32_t barCount = (slotSize + barSpacing) / (barWidth + barSpacing);
        std::vector<float> loudness(barCount, 0.0f);
        for (int32_t i = 0; i < barCount && sampleCount > 0; i++) {
            auto start = sampleCount * size_t(i) / size_t(barCount);
            auto end = glm::max(sampleCount * size_t(i + 1) / size_t(barCount), start + 1);
            double sum = 0.0;
            for (auto j = start; j < end; j++)
                sum += double(samples[j]) * double(samples[j]);
            loudness[i] = float(std::sqrt(sum / double(end - start)));
        }
        SDL_free(converted);

        auto maxLoudness = *std::max_element(loudness.begin(), loudness.end());

        image.width = slotSize;
        image.height = slotSize;
        image.data.assign(size_t(slotSize) * slotSize * 4, 0);

        auto barsWidth = barCount * (barWidth + barSpacing) - barSpacing;
        auto startX = (slotSize - barsWidth) / 2;
        auto center = float(slotSize) * 0.5f;
        auto maxHeight = float(slotSize) * 0.6f;

        for (int32_t i = 0; i < barCount; i++) {
            auto value = maxLoudness > 0.0f ? loudness[i] / maxLoudness : 0.0f;
            // Quiet parts stay visible as a line
            auto halfHeight = glm::max(0.5f * maxHeight * value, 1.5f);
            for (int32_t y = 0; y < slotSize; y++) {
                // Covered part of the pixel, such that the ends of the bars are smooth
                auto pixelMin = float(y);
                auto coverage = glm::clamp(glm::min(pixelMin + 1.0f, center + halfHeight) -
                    glm::max(pixelMin, center - halfHeight), 0.0f, 1.0f);
                if (coverage <= 0.0f)
                    continue;
                for (int32_t x = 0; x < barWidth; x++) {
                    auto idx = (size_t(y) * slotSize + size_t(startX + i * (barWidth + barSpacing) + x)) * 4;
                    image.data[idx + 0] = 255;
                    image.data[idx + 1] = 255;
                    image.data[idx + 2] = 255;
                    image.data[idx + 3] = uint8_t(coverage * 255.0f + 0.5f);
                }
            }
        }

        return image;

    }

}
