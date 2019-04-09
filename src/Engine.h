#ifndef AE_ENGINE_H
#define AE_ENGINE_H

#include "System.h"
#include "Window.h"
#include "Camera.h"
#include "scene/Scene.h"
#include "RenderTarget.h"
#include "shader/Shader.h"
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
         * @param windowTitle The title of your window
         * @param x The x position of the window on the screen in pixels
         * @param y The y position of the window on the screen in pixels
         * @param width The width of the window in pixels
         * @param height The height of the window in pixels
         * @param flags Window flags. See {@link Window.h} for more.
         * @return A pointer to a window object.
         * @note All file paths handed over to the engine should be relative the asset directory
         */
		static Window *Init(std::string assetDirectory, std::string shaderDirectory, std::string windowTitle,
							int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags = AE_WINDOW_FULLSCREEN);

		/**
         * Returns the dimensions of the screen in pixels.
         * @param width A pointer to an integer.
         * @param height A pointer to an integer.
         */
		static void GetScreenSize(int32_t *width, int32_t *height);

		/**
		 * Returns the time in seconds.
		 * @return The time as a float in seconds.
		 */
		static float GetClock();

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
         * Updates the engine and the system events. Must be called in thread
         * where the engine was initialized.
         */
		static void Update(float deltaTime);

	};

}

#endif