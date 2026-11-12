#pragma once

#include "Content.h"

#include "System.h"
#include "Viewport.h"
#include "Material.h"
#include "common/Hash.h"
#include "jobsystem/JobSystem.h"
#include "mesh/Mesh.h"
#include "materialgraph/MaterialGraph.h"
#include "resource/Resource.h"
#include "scene/Scene.h"
#include "texture/Texture2D.h"
#include "renderer/MainRenderer.h"
#include "renderer/target/RenderTarget.h"

#include <imgui.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Atlas::Editor {

    // Small pictures of content, e.g. of textures, meshes and materials. All previews share one texture of a fixed size,
    // the ones which weren't shown for the longest time make room for new ones. Images are decoded in the background,
    // meshes and materials are rendered one after another. Generated previews are cached on disk (.cache/previews/)
    class ContentPreviews {

    public:
        struct Preview {
            ImTextureID texture = {};
            ImVec2 uvMin;
            ImVec2 uvMax;
            // In pixels of the preview, which has the aspect ratio of the content
            ImVec2 size;
            // Previews of fonts and audio only have an alpha channel and are tinted like icons
            bool tintable = false;
            // Rendered meshes and materials have no background
            bool transparent = false;
            // Previews fade in once they are generated
            float alpha = 1.0f;
        };

        ContentPreviews() = default;

        ~ContentPreviews();

        static bool IsSupported(ContentType type);

        // Requests the preview of content which is visible in this frame. False while there is no preview (yet)
        bool Get(const std::string& assetPath, ContentType type, uintmax_t size, int64_t lastWriteTime, Preview& preview);

        // Generates, uploads and evicts previews, called once per frame after the previews were requested
        void Update();

        // Called every frame, also while the previews aren't shown. Stops rendering and releases the renderer
        // once the previews weren't updated for a while
        void ReleaseUnused();

        static constexpr int32_t slotSize = 160;

    private:
        // The preview texture has a few mip levels, such that small icons don't flicker
        class PreviewAtlas : public Texture::Texture2D {
        public:
            PreviewAtlas(int32_t size, uint32_t mipLevels);
        };

        enum class State {
            Queued = 0,
            Loading,
            Rendering,
            Ready,
            Failed
        };

        struct Image {
            int32_t width = 0;
            int32_t height = 0;
            // RGBA with sRGB encoded colors, as they are shown
            std::vector<uint8_t> data;
        };

        struct Entry {
            Hash version = 0;
            ContentType type = ContentType::None;
            State state = State::Queued;

            int32_t slot = -1;
            ivec2 size = ivec2(0);
            bool hasImage = false;

            int64_t lastRequestFrame = -1;
            float readyTime = 0.0f;
            // Failures which might go away, e.g. a timeout while loading, are tried again after this time
            float retryTime = 0.0f;
        };

        struct Result {
            std::string assetPath;
            Hash version = 0;
            // Meshes and materials which aren't cached are rendered
            bool needsRender = false;
            bool failed = false;
            Image image;
            // All mip levels of the slot, ready to be uploaded
            std::vector<uint8_t> slotLevels;
        };

        struct RenderJob {
            std::string assetPath;
            Hash version = 0;
            ContentType type = ContentType::None;
            // Only counts while the job is worked on, e.g. not while the editor blocks rendering
            float activeTime = 0.0f;

            ResourceHandle<Mesh::Mesh> mesh;
            ResourceHandle<Material> material;
            ResourceHandle<MaterialGraph::Graph> graph;

            Ref<Scene::Scene> scene;
            Scene::Entity root;
            Scene::Entity camera;

            int32_t renderedFrames = 0;
            // Rendered after a timeout, e.g. with textures which didn't load. Such previews aren't cached
            bool complete = true;
            // The frames are submitted at the end of the frame, the result is read in the next one
            bool waitForCapture = false;
        };

        void Init();

        void StartJobs();

        void ProcessResults();

        void Upload(const std::vector<std::pair<Entry*, const Result*>>& uploads);

        int32_t AllocateSlot();

        void UpdateRendering();

        bool StartRender(RenderJob& job);

        void CreateRenderScene(RenderJob& job);

        void RenderFrame(RenderJob& job);

        Image Capture();

        void FinishRender(bool failed, Image image);

        void CancelRender();

        void Evict();

        ivec2 GetSlotPosition(int32_t slot) const;

        static Result Generate(const std::string& assetPath, ContentType type, Hash version);

        // Mip levels of the whole slot with the image in its corner, the rest of the slot is transparent
        static std::vector<uint8_t> BuildSlotLevels(const Image& image);

        // Files a rendered preview depends on, e.g. the textures of a material
        static std::vector<std::string> GetDependencies(RenderJob& job);

        static std::string GetCachePath(Hash version);

        static void WriteCache(Hash version, const Image& image, const std::vector<std::string>& dependencies);

        static bool ReadCache(Hash version, bool checkDependencies, Image& image);

        static Image LoadTexture(const std::string& assetPath);

        static Image LoadEnvironmentTexture(const std::string& assetPath);

        static Image LoadFont(const std::string& assetPath);

        static Image LoadAudio(const std::string& assetPath);

        static bool IsRendered(ContentType type);

        Ref<PreviewAtlas> atlas;

        std::unordered_map<std::string, Entry> entries;
        std::vector<std::string> requested;
        std::vector<int32_t> freeSlots;
        int32_t nextSlot = 0;
        int64_t frame = 0;
        float lastUpdateTime = 0.0f;

        JobGroup jobGroup{ "Content previews", JobPriority::Low };
        std::atomic_int32_t runningJobs = 0;
        std::mutex resultMutex;
        std::vector<Result> results;

        // Content which has to be rendered, one after another
        std::vector<std::string> renderQueue;
        Scope<RenderJob> renderJob;

        Ref<Renderer::MainRenderer> renderer;
        Ref<Renderer::RenderTarget> renderTarget;
        Ref<Viewport> renderViewport;
        Texture::Texture2D renderTexture;
        float lastRenderTime = 0.0f;

        static constexpr int32_t slotsPerRow = 13;
        static constexpr uint32_t mipLevels = 6;
        // Decoding large images needs a lot of memory, so only a few run at once
        static constexpr int32_t maxRunningJobs = 2;
        // Rendered at a higher resolution, the edges against the transparent background are smooth then
        static constexpr int32_t renderScale = 2;
        static constexpr int32_t renderFrames = 16;
        static constexpr float renderTimeout = 20.0f;
        static constexpr float retryDelay = 30.0f;
        // The renderer is released after a while without anything to render, rendering stops once the previews
        // weren't updated for a moment, e.g. after the content browser was closed
        static constexpr float rendererIdleTime = 30.0f;
        static constexpr float hiddenTime = 1.0f;
        static constexpr float fadeTime = 0.15f;

    };

}
