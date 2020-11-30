#ifndef AE_APP_H
#define AE_APP_H

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>

#include <renderer/RayTracingRenderer.h>

#ifndef AE_OS_ANDROID
#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI
#else
#define WINDOW_FLAGS AE_WINDOW_FULLSCREEN
#endif

class App : public Atlas::EngineInstance {

public:
	App() : EngineInstance("Atlas Engine", 1280, 720, WINDOW_FLAGS) {}

	virtual void LoadContent() final;

	virtual void UnloadContent() final;

	virtual void Update(float deltaTime) final;

	virtual void Render(float deltaTime) final;

private:
	void DisplayLoadingScreen();

	Atlas::Renderer::RayTracerRenderTarget rayTraceTarget;
	Atlas::Viewport viewport;

	Atlas::Font font;

	Atlas::Camera camera;

	Atlas::Scene::Scene scene;

	Atlas::Mesh::Mesh mesh;
	Atlas::Actor::StaticMeshActor actor;

	Atlas::Input::MouseHandler mouseHandler;
	Atlas::Input::KeyboardHandler keyboardHandler;
	Atlas::Input::ControllerHandler controllerHandler;
	Atlas::Input::TouchHandler touchHandler;

	bool useControllerHandler;

	Atlas::Renderer::RayTracingRenderer rayTracingRenderer;

};

#endif