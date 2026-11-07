#include "StreamingManager.h"
#include "../graphics/GraphicsDevice.h"
#include "../profiler/CPUProfiler.h"

namespace Atlas::Streaming {

    StreamingSettings StreamingManager::settings;
    int32_t StreamingManager::lastUpdateFrame = -1;

    void StreamingManager::Update() {

        auto frameIndex = Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex();
        if (frameIndex == lastUpdateFrame)
            return;
        lastUpdateFrame = frameIndex;

        CPUProfiler::Scope profilerScope("Streaming");

        // Meshes keep their geometry until their impostors are resident
        TextureStreaming::residency.Update(settings.textures);
        ImpostorStreaming::residency.Update(settings.impostors);
        MeshStreaming::Update(settings.meshes);

    }

    StreamingManager::Stats StreamingManager::GetStats() {

        return Stats {
            .textures = TextureStreaming::residency.GetStats(),
            .impostors = ImpostorStreaming::residency.GetStats(),
            .meshes = MeshStreaming::residency.GetStats()
        };

    }

    void StreamingManager::Shutdown() {

        TextureStreaming::residency.Shutdown();
        ImpostorStreaming::residency.Shutdown();
        MeshStreaming::residency.Shutdown();

    }

}
