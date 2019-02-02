#ifndef ENGINE_H
#define ENGINE_H

#include "System.h"
#include "Window.h"
#include "Camera.h"
#include "Scene.h"
#include "RenderTarget.h"
#include "shader/Shader.h"
#include "renderer/MasterRenderer.h"
#include "events/EngineEventHandler.h"
#include "lighting/DirectionalLight.h"
#include "lighting/PointLight.h"
#include "loader/AssetLoader.h"

class Engine {

public:
	/**
	 * Initializes the engine
	 * @param assetDirectory The directory where all assets are located
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
	static Window* Init(std::string assetDirectory, std::string shaderDirectory, std::string windowTitle,
		int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags = AE_WINDOW_FULLSCREEN);

	/**
	 * 
	 * @param width
	 * @param height
	 */
	static void GetScreenSize(int32_t* width, int32_t* height);

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
	static void Update();

};

#endif