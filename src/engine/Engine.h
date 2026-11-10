#pragma once

#include "System.h"
#include "Window.h"
#include "Clock.h"
#include "Log.h"
#include "resource/ResourceManager.h"
#include "scene/Scene.h"
#include "renderer/MainRenderer.h"
#include "events/EventManager.h"
#include "audio/AudioManager.h"
#include "loader/AssetLoader.h"
#include "common/RandomHelper.h"
#include "jobsystem/JobSystem.h"

#include <functional>
#include <optional>

namespace Atlas {

    struct EngineConfig {
        /**
         * Asset directory which will be used to load data relative to that path
         */
        std::string assetDirectory = "data";

        /**
         * Shader directory, all shaders are loaded relative to that path
         * @note This path must be relative to the asset directory
         */
        std::string shaderDirectory = "shader";

        /**
         * Configures validation layers and robustness of the graphics device
         */
        Graphics::DeviceDesc deviceDesc;

        /*
         *
         */
        JobSystemConfig jobSystemConfig;

        /**
         * Runs before the engine is initialized with the command line arguments, e.g. for command line modes that
         * need neither a window nor the GPU. Returning an exit code ends the application right away
         */
        std::function<std::optional<int32_t>(const std::vector<std::string>& args)> commandLineTool;
    };

    class Engine {

    public:
        /**
         * Initializes the engine
         * @param config Engine configuration
         * @note All file paths handed over to the engine should be relative the asset directory
         */
        static void Init(EngineConfig config);

        /**
         * Shuts down the engine
         */
        static void Shutdown();

        /**
         * Updates the engine and the system events. Must be called in thread
         * where the engine was initialized.
         * @param renderThread With a render thread, BeginRenderFrame() is called once the last frame is rendered.
         */
        static void Update(bool renderThread = false);

        /**
         * Starts the frame of the profiler and reloads changed shaders. Rendering must not run meanwhile.
         */
        static void BeginRenderFrame();

        /**
         * Returns the size of the screen.
         * @return An 2-component integer vector where x is the width and y is the height.
         */
        static ivec2 GetScreenSize();

        static Window* DefaultWindow;

    };

}