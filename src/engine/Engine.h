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
         * Configures which messages of the validation layers to log
         */
        Log::Severity validationLayerSeverity = Log::SEVERITY_LOW;
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
         */
        static void Update();

        /**
         * Returns the size of the screen.
         * @return An 2-component integer vector where x is the width and y is the height.
         */
        static ivec2 GetScreenSize();

        static Window* DefaultWindow;

    };

}