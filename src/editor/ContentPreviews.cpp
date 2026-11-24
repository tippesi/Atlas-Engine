#include "ContentPreviews.h"
#include "ContentDiscovery.h"
#include "DataCreator.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Theme.h"

#include "Clock.h"
#include "Serializer.h"
#include "common/Path.h"
#include "graphics/GraphicsDevice.h"
#include "loader/AssetLoader.h"
#include "loader/KtxLoader.h"
#include "loader/MaterialGraphLoader.h"
#include "loader/MaterialLoader.h"
#include "physics/PhysicsWorld.h"
#include "profiler/CPUProfiler.h"
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
        case ContentType::MaterialSubgraph:
        case ContentType::Prefab: return true;
        default: return false;
        }

    }

    bool ContentPreviews::IsRendered(ContentType type) {

        return type == ContentType::Mesh || type == ContentType::Material ||
            type == ContentType::MaterialGraph || type == ContentType::MaterialSubgraph || type == ContentType::Prefab;

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

        lastRequestTime = Clock::Get();
        requestedThisFrame = true;

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

    bool ContentPreviews::Get(const std::string& assetPath, Preview& preview) {

        // The file as the content discovery found it, such that the content browser requests the same version
        auto normalizedPath = Common::Path::Normalize(assetPath);
        auto directoryPath = Common::Path::GetDirectory(normalizedPath);
        auto dataDirectory = Loader::AssetLoader::GetDataDirectory();
        auto directory = ContentDiscovery::GetDirectory(directoryPath.empty() ? dataDirectory : dataDirectory + "/" + directoryPath);
        if (!directory)
            return false;

        for (const auto& file : directory->files) {
            if (file.assetPath == normalizedPath)
                return Get(file.assetPath, file.type, file.size, file.lastWriteTime, preview);
        }

        return false;

    }

    void ContentPreviews::Render(const std::string& assetPath, ImVec2 size) {

        auto min = ImGui::GetCursorScreenPos();
        auto max = ImVec2(min.x + size.x, min.y + size.y);
        ImGui::Dummy(size);
        if (!ImGui::IsItemVisible())
            return;

        auto fileType = Common::Path::GetFileType(assetPath);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);
        auto typeIter = Content::contentTypeMapping.find(fileType);
        auto type = typeIter != Content::contentTypeMapping.end() ? typeIter->second : ContentType::None;
        auto iconTexture = Singletons::imguiWrapper->GetTextureId(&GetIcon(type));

        auto drawList = ImGui::GetWindowDrawList();
        auto rounding = std::clamp(std::min(size.x, size.y) * 0.12f, 2.0f, Theme::overlayRounding);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::PreviewBackground()), rounding);

        Preview preview;
        if (Get(assetPath, preview)) {
            Draw(drawList, preview, type, min, max, iconTexture, false, rounding);
        }
        else {
            auto inset = iconInset * std::min(size.x, size.y);
            drawList->AddImage(iconTexture, ImVec2(min.x + inset, min.y + inset),
                ImVec2(max.x - inset, max.y - inset), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(Theme::ContentIcon(type, iconAlpha)));
        }

    }

    void ContentPreviews::Draw(ImDrawList* drawList, const Preview& preview, ContentType contentType, ImVec2 min, ImVec2 max,
        ImTextureID iconTexture, bool framed, float rounding) {

        auto size = ImVec2(max.x - min.x, max.y - min.y);

        // The icon is replaced smoothly once the preview is generated
        if (preview.alpha < 1.0f) {
            auto inset = iconInset * std::min(size.x, size.y);
            drawList->AddImage(iconTexture, ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, max.y - inset),
                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(Theme::ContentIcon(contentType, iconAlpha * (1.0f - preview.alpha))));
        }

        // Fonts and audio are only a shape, they get some room around them
        if (preview.tintable) {
            auto inset = 0.12f * std::min(size.x, size.y);
            min = ImVec2(min.x + inset, min.y + inset);
            max = ImVec2(max.x - inset, max.y - inset);
            size = ImVec2(max.x - min.x, max.y - min.y);
        }

        // Fitted into the area with the aspect ratio of the content
        auto scale = std::min(size.x / std::max(preview.size.x, 1.0f), size.y / std::max(preview.size.y, 1.0f));
        auto fitted = ImVec2(std::floor(preview.size.x * scale), std::floor(preview.size.y * scale));
        auto previewMin = ImVec2(std::floor(min.x + 0.5f * (size.x - fitted.x)), std::floor(min.y + 0.5f * (size.y - fitted.y)));
        auto previewMax = ImVec2(previewMin.x + fitted.x, previewMin.y + fitted.y);

        if (preview.tintable) {
            drawList->AddImage(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(Theme::ContentIcon(contentType, 0.85f * preview.alpha)));
        }
        else if (preview.transparent) {
            drawList->AddImage(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, preview.alpha)));
        }
        else {
            if (framed)
                rounding = std::clamp(std::min(fitted.x, fitted.y) * 0.06f, 1.0f, Theme::overlayRounding);
            drawList->AddImageRounded(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, preview.alpha)), rounding);
            // Dark images stand out from the background
            if (framed)
                drawList->AddRect(previewMin, previewMax, ImGui::GetColorU32(ImGuiCol_Border, 0.6f * preview.alpha), rounding);
        }

    }

    Texture::Texture2D& ContentPreviews::GetIcon(ContentType contentType) {

        auto& icons = Singletons::icons;

        switch (contentType) {
        case ContentType::Audio: return icons->GetTintable(IconType::Audio);
        case ContentType::Mesh: return icons->GetTintable(IconType::Mesh);
        case ContentType::MeshSource: return icons->GetTintable(IconType::MeshSource);
        case ContentType::Material: return icons->GetTintable(IconType::Material);
        case ContentType::MaterialGraph:
        case ContentType::MaterialSubgraph: return icons->GetTintable(IconType::MaterialGraph);
        case ContentType::Scene: return icons->GetTintable(IconType::Scene);
        case ContentType::Font: return icons->GetTintable(IconType::Font);
        case ContentType::Prefab: return icons->GetTintable(IconType::Prefab);
        case ContentType::Texture: return icons->GetTintable(IconType::Image);
        case ContentType::EnvironmentTexture: return icons->GetTintable(IconType::EnvironmentImage);
        case ContentType::Terrain: return icons->GetTintable(IconType::Terrain);
        default: return icons->GetTintable(IconType::Document);
        }

    }

    void ContentPreviews::Update() {

        if (!atlas && !requested.empty())
            Init();

        if (atlas) {
            ProcessResults();
            StartJobs();
            UpdateRendering();
            if (requestedThisFrame)
                Evict();
        }

        requested.clear();
        // Only frames which show previews count, such that they aren't forgotten while nothing shows them
        if (requestedThisFrame)
            frame++;
        requestedThisFrame = false;

    }

    void ContentPreviews::ReleaseUnused() {

        auto now = Clock::Get();

        // Nothing is rendered for previews which aren't shown, the content is rendered again once it is visible
        if (renderJob && now - lastRequestTime > hiddenTime)
            CancelRender();

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
                FinishRender(true);
        }

        if (!renderJob)
            return;

        // Waiting for the editor doesn't count towards the timeout
        if (Singletons::blockingOperation->block)
            return;

        auto& job = *renderJob;
        job.activeTime += Clock::GetDelta();

        // The copy of the last frame is read once the device finished that frame, nothing waits for it
        if (job.readbackFrame >= 0) {
            auto device = Graphics::GraphicsDevice::DefaultDevice;
            if (device->GetFrameIndex() - job.readbackFrame > FRAME_DATA_COUNT) {
                CPUProfiler::Scope scope("Read preview");
                FinishRender(false);
            }
            return;
        }

        auto timedOut = job.activeTime > renderTimeout;
        auto failedToLoad = [](const auto& handle) {
            return handle.IsValid() && handle.GetResource()->errorOnLoad;
            };

        if (failedToLoad(job.mesh) || failedToLoad(job.material) || failedToLoad(job.graph)) {
            FinishRender(true);
            return;
        }

        if (!job.root.IsValid()) {
            auto isLoaded = [](const auto& handle) {
                return !handle.IsValid() || handle.IsLoaded();
                };
            if (!isLoaded(job.mesh) || !isLoaded(job.material) || !isLoaded(job.graph)) {
                if (timedOut)
                    FinishRender(true);
                return;
            }

            CPUProfiler::Scope scope("Add preview content");
            if (!renderer)
                InitRendering();
            AddContent(job);
            if (!job.root.IsValid()) {
                FinishRender(true);
                return;
            }
        }

        // Textures of the materials are loaded in the background
        if (!renderScene->IsFullyLoaded()) {
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
        case ContentType::MaterialSubgraph: {
            // The preview is made of the outputs of the subgraph, so it is needed right away
            auto subgraph = ResourceManager<MaterialGraph::Graph>::GetOrLoadResourceWithLoader(job.assetPath,
                ResourceOrigin::User, Loader::MaterialGraphLoader::LoadGraph);
            if (!subgraph.IsLoaded())
                return false;
            job.graph = ResourceHandle<MaterialGraph::Graph>(MaterialGraph::Graph::CreateSubgraphPreview(subgraph));
            return true;
        }
        case ContentType::Prefab:
            return true;
        default:
            return false;
        }

    }

    void ContentPreviews::InitRendering() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        auto renderSize = slotSize * renderScale;
        renderer = CreateRef<Renderer::MainRenderer>();
        renderer->Init(device);
        renderTarget = CreateRef<Renderer::RenderTarget>(renderSize, renderSize);
        renderViewport = CreateRef<Viewport>(0, 0, renderSize, renderSize);
        renderTexture = Texture::Texture2D(renderSize, renderSize, VK_FORMAT_R16G16B16A16_SFLOAT);

        auto readbackDesc = Graphics::BufferDesc{
            .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .domain = Graphics::BufferDomain::Host,
            .hostAccess = Graphics::BufferHostAccess::Random,
            .size = size_t(renderSize) * size_t(renderSize) * sizeof(float16) * 4
        };
        colorReadback = device->CreateBuffer(readbackDesc);

        // The depth has the scaled resolution of the render target
        auto depthTexture = renderTarget->GetData(Renderer::FULL_RES)->depthTexture;
        depthSize = ivec2(depthTexture->width, depthTexture->height);
        readbackDesc.size = size_t(depthSize.x) * size_t(depthSize.y) * sizeof(float);
        depthReadback = device->CreateBuffer(readbackDesc);

        renderScene = CreateRef<Scene::Scene>("Content preview");
        // Like the scenes of the editor, prefabs with rigid bodies or scripts expect one. It never runs
        renderScene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        renderScene->physicsWorld->pauseSimulation = true;

        auto rootGroup = renderScene->CreatePrefab<Scene::Prefabs::Group>("Root");
        rootGroup.GetComponent<HierarchyComponent>().root = true;

        renderCamera = renderScene->CreateEntity();
        auto& camera = renderCamera.AddComponent<CameraComponent>(40.0f, 1.0f, 0.01f, 100.0f,
            vec3(0.0f), vec2(glm::pi<float>() + 0.6f, -0.55f));
        camera.isMain = true;

        renderLight = renderScene->CreateEntity();

    }

    void ContentPreviews::AddContent(RenderJob& job) {

        Scene::Entity entity;
        if (job.type == ContentType::Mesh) {
            entity = renderScene->CreatePrefab<Scene::Prefabs::MeshInstance>(job.mesh, false);
            // Skinned meshes need an animation component to be shown
            if (job.mesh->data.IsSkinned())
                entity.AddComponent<AnimationComponent>();
        }
        else if (job.type == ContentType::Material || job.graph.IsValid()) {
            // Graphs are shown with a material which only exists for the preview
            if (job.graph.IsValid()) {
                job.material = ResourceHandle<Material>(CreateRef<Material>());
                job.material->graph = job.graph;
            }
            // Fewer vertices than the sphere of the material editor, a small preview doesn't show displacement in detail
            auto sphere = ResourceHandle<Mesh::Mesh>(DataCreator::CreateSphere(job.material, 64, 96));
            job.mesh = sphere;
            entity = renderScene->CreatePrefab<Scene::Prefabs::MeshInstance>(sphere, false);
        }
        else if (job.type == ContentType::Prefab) {
            try {
                entity = Serializer::DeserializePrefab(renderScene, job.assetPath);
            }
            catch (const std::exception&) {
                entity = Scene::Entity();
            }
            if (!entity.IsValid())
                return;

            // A preview only shows the prefab, scripts aren't executed and sounds aren't played
            auto removeAll = [&]<class Comp>() {
                std::vector<ECS::Entity> entities;
                for (auto componentEntity : renderScene->entityManager.GetSubset<Comp>())
                    entities.push_back(componentEntity);
                for (auto componentEntity : entities)
                    renderScene->entityManager.Erase<Comp>(componentEntity);
                };
            removeAll.template operator()<LuaScriptComponent>();
            removeAll.template operator()<AudioComponent>();
            removeAll.template operator()<AudioVolumeComponent>();
        }

        job.root = entity;

    }

    void ContentPreviews::RemoveContent(RenderJob& job) {

        if (job.root.IsValid())
            renderScene->DestroyEntity(job.root, true);
        job.root = Scene::Entity();

    }

    void ContentPreviews::RenderFrame(RenderJob& job) {

        // A frame which the renderer skips, e.g. while the window is minimized, doesn't count
        auto device = Graphics::GraphicsDevice::DefaultDevice;
        if (!device->swapChain->isComplete)
            return;

        const float deltaTime = 1.0f / 60.0f;

        CPUProfiler::Scope scope("Update preview scene");
        if (job.renderedFrames == 0) {
            // The camera direction and the bounds of prefabs are known after an update
            renderScene->Update(deltaTime, renderCamera);

            Volume::AABB bounds(vec3(-1.0f), vec3(1.0f));
            if (job.type == ContentType::Prefab)
                bounds = renderScene->GetHierarchyBounds(job.root).value_or(bounds);
            else if (job.mesh.IsLoaded())
                bounds = job.mesh->data.aabb;

            // The sphere of materials fills the preview, other content fits in with its bounding sphere
            auto center = 0.5f * (bounds.min + bounds.max);
            auto radius = glm::max(0.5f * glm::length(bounds.max - bounds.min), 0.01f);
            if (job.type == ContentType::Material || job.graph.IsValid())
                radius = 1.05f;

            auto& camera = renderCamera.GetComponent<CameraComponent>();
            auto distance = radius / std::sin(glm::radians(camera.fieldOfView) * 0.5f);
            camera.location = center - camera.direction * distance;
            camera.nearPlane = 0.01f * distance;
            camera.farPlane = 4.0f * (distance + radius);

            // Prefabs might bring their own light, the light of the preview is only there without one
            auto hasOwnLight = false;
            for (auto lightEntity : renderScene->GetSubset<LightComponent>())
                hasOwnLight |= lightEntity != renderLight;

            auto hasLight = renderLight.HasComponent<LightComponent>();
            if (hasOwnLight && hasLight) {
                renderLight.RemoveComponent<LightComponent>();
            }
            else if (!hasOwnLight && !hasLight) {
                auto& light = renderLight.AddComponent<LightComponent>(LightType::DirectionalLight);
                light.properties.directional.direction = vec3(-0.5f, -1.0f, -1.0f);
                light.color = vec3(255, 236, 209) / 255.0f;
                light.intensity = 10.0f;
                light.AddDirectionalShadow(distance + 2.0f * radius, 3.0f, 1024, 0.05f, 1, 0.9f);
                light.isMain = true;
            }
            if (!hasOwnLight)
                renderLight.GetComponent<LightComponent>().shadow->distance = distance + 2.0f * radius;
        }

        renderScene->Update(deltaTime, renderCamera);

        scope.Next("Render preview scene");
        renderer->Update();
        renderer->RenderScene(renderViewport, renderTarget, renderScene, {}, &renderTexture);
        job.renderedFrames++;

        // Graphs compile their pipelines in the background, the frames only count once they are used
        auto graphPending = false;
        for (const auto& mesh : renderScene->GetMeshes()) {
            if (!mesh.IsLoaded())
                continue;
            for (const auto& subData : mesh->data.subData)
                graphPending |= subData.mainPipelines.graphPending;
        }
        if (graphPending) {
            if (job.activeTime <= renderTimeout)
                job.renderedFrames = 1;
            else
                job.complete = false;
        }

        if (job.renderedFrames >= renderFrames) {
            scope.Next("Copy preview");
            CopyForReadback();
            job.readbackFrame = device->GetFrameIndex();
        }

    }

    void ContentPreviews::CopyForReadback() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        // The swap of the render target moved the depth of the last frame into the history
        auto depthImage = renderTarget->GetHistoryData(Renderer::FULL_RES)->depthTexture->image;
        auto colorImage = renderTexture.image;

        auto commandList = device->GetCommandList(Graphics::QueueType::GraphicsQueue);
        commandList->BeginCommands();

        auto colorLayout = colorImage->layout;
        auto colorAccess = colorImage->accessMask;
        auto depthLayout = depthImage->layout;
        auto depthAccess = depthImage->accessMask;

        Graphics::ImageBarrier transferBarriers[] = {
            { colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT },
            { depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT },
        };
        commandList->PipelineBarrier(transferBarriers, {}, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        commandList->CopyImageToBuffer(colorImage, colorReadback);
        commandList->CopyImageToBuffer(depthImage, depthReadback);

        Graphics::ImageBarrier restoreBarriers[] = {
            { colorImage, colorLayout, colorAccess },
            { depthImage, depthLayout, depthAccess },
        };
        commandList->PipelineBarrier(restoreBarriers, {}, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

        commandList->EndCommands();
        device->SubmitCommandList(commandList);

    }

    ContentPreviews::Image ContentPreviews::ProcessCapture(const std::vector<float16>& colors, const std::vector<float>& depth,
        ivec2 depthSize) {

        Image image;

        auto width = slotSize * renderScale;
        auto getDepth = [&](int32_t x, int32_t y) {
            return depth[size_t(y * depthSize.y / width) * depthSize.x + size_t(x * depthSize.x / width)];
            };

        // The content fits into the view, so a corner shows the background
        auto background = getDepth(0, 0);

        // The post processing already encoded the colors for the display, they are averaged as linear colors
        auto size = ivec2(slotSize);
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
                        auto pixel = size_t(pixelY) * width + size_t(pixelX);
                        auto color = [&](size_t channel) {
                            return SrgbToLinear(glm::detail::toFloat32(colors[pixel * 4 + channel]));
                            };
                        sum += vec4(color(0), color(1), color(2), 1.0f);
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

    void ContentPreviews::FinishRender(bool failed) {

        auto& job = *renderJob;

        auto iter = entries.find(job.assetPath);
        if (iter != entries.end() && iter->second.version == job.version) {
            if (failed) {
                // Loading or rendering might work later, e.g. after a long import
                iter->second.state = State::Failed;
                iter->second.retryTime = Clock::Get() + retryDelay;
            }
            else {
                std::vector<float16> colors(colorReadback->size / sizeof(float16));
                std::vector<float> depth(depthReadback->size / sizeof(float));
                colorReadback->Invalidate();
                std::memcpy(colors.data(), colorReadback->Map(), colorReadback->size);
                colorReadback->Unmap();
                depthReadback->Invalidate();
                std::memcpy(depth.data(), depthReadback->Map(), depthReadback->size);
                depthReadback->Unmap();

                // Incomplete previews are shown, but not cached
                auto dependencies = job.complete ? GetDependencies() : std::vector<std::string>();
                JobSystem::Execute(jobGroup, [this, assetPath = job.assetPath, version = job.version, complete = job.complete,
                    dependencies, colors = std::move(colors), depth = std::move(depth), depthSize = depthSize](JobData&) {
                    auto image = ProcessCapture(colors, depth, depthSize);
                    auto result = Result{ .assetPath = assetPath, .version = version, .failed = image.data.empty() };
                    if (!result.failed) {
                        if (complete)
                            WriteCache(version, image, dependencies);
                        result.slotLevels = BuildSlotLevels(image);
                        result.image = std::move(image);
                    }
                    std::scoped_lock lock(resultMutex);
                    results.push_back(std::move(result));
                    });
            }
        }

        RemoveContent(job);
        renderJob.reset();

    }

    void ContentPreviews::CancelRender() {

        auto iter = entries.find(renderJob->assetPath);
        if (iter != entries.end() && iter->second.version == renderJob->version)
            iter->second.state = State::Queued;

        RemoveContent(*renderJob);
        renderJob.reset();

    }

    std::vector<std::string> ContentPreviews::GetDependencies() {

        std::set<std::string> paths;
        auto add = [&](const auto& handle) {
            if (handle.IsValid() && !handle.IsGenerated())
                paths.insert(handle.GetResource()->path);
            };

        for (const auto& mesh : renderScene->GetMeshes())
            add(mesh);

        for (const auto& material : renderScene->GetMaterials()) {
            add(material);
            add(material->graph);
            for (const auto& map : material->GetMaps())
                add(map);
            for (const auto& map : material->GetGraphTextures())
                add(map);
        }

        // The file of the preview itself is part of the version
        paths.erase(renderJob->assetPath);

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
            // Only the levels up to the preview size are read and the one closest to it is decoded
            ktxImage = Loader::KtxLoader::DecodeImage(assetPath, slotSize);
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
