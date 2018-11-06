#ifndef ENGINE_H
#define ENGINE_H

#include "system.h"
#include "window.h"
#include "camera.h"
#include "scene.h"
#include "rendertarget.h"
#include "shader/shader.h"
#include "renderer/masterrenderer.h"

namespace Engine {

	Window* Init(const char* title, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags = WINDOW_FULLSCREEN);

	void LockFramerate();

	void UnlockFramerate();

}

#endif