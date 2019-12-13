#ifndef AE_ENGINE_H
#define AE_ENGINE_H

#include "System.h"
#include "Window.h"
#include "Context.h"
#include "Camera.h"
#include "Clock.h"
#include "scene/Scene.h"
#include "RenderTarget.h"
#include "shader/Shader.h"
#include "shader/ShaderManager.h"
#include "renderer/MasterRenderer.h"
#include "events/EventManager.h"
#include "audio/AudioManager.h"
#include "lighting/DirectionalLight.h"
#include "lighting/PointLight.h"
#include "loader/AssetLoader.h"

namespace Atlas {

	class Engine {

	public:
		/**
         * Initializes the engine
         * @param assetDirectory The directory where all assets are located (needs to be relative to the runtime directory)
         * @param shaderDirectory The directory where all the shader files are located relative to the asset directory
         * @note All file paths handed over to the engine should be relative the asset directory
         */
		static Context* Init(std::string assetDirectory, std::string shaderDirectory);

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

        /**
         * Locks the frame rate to the next available target frame rate of the monitor
         * like min(availableTargetFramerate, possibleFramerate).
         */
        static void LockFramerate();

        /**
         * Unlocks the framerate.
         */
        static void UnlockFramerate();

		/**
		 * This window is there by default and is not visible.
		 */
		static SDL_Window* defaultWindow;
		static Context* defaultContext;

	};

}

#endif