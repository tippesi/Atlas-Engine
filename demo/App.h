#ifndef AE_APP_H
#define AE_APP_H

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <Imgui/ImguiWrapper.h>

#include <renderer/PathTracingRenderer.h>

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

class App : public Atlas::EngineInstance {

public:
	App() : EngineInstance("Atlas Engine", 1280, 720, WINDOW_FLAGS) {}

	virtual void LoadContent() final;

	virtual void UnloadContent() final;

	virtual void Update(float deltaTime) final;

	virtual void Render(float deltaTime) final;

private:
	enum SceneSelection {
		CORNELL = 0,
		SPONZA,
		BISTRO,
		SANMIGUEL,
		MEDIEVAL
	};

	void DisplayLoadingScreen();

	bool IsSceneAvailable(SceneSelection selection);
	bool LoadScene();
	void UnloadScene();

	void AddProbeActors();
	void RemoveProbeActors();

	void SetResolution(int32_t width, int32_t height);

	SceneSelection sceneSelection = SPONZA;

	Atlas::Renderer::PathTracerRenderTarget pathTraceTarget;
	Atlas::RenderTarget* renderTarget;
	Atlas::Viewport viewport;

	Atlas::Font font;

	Atlas::Camera camera;

	Atlas::Scene::Scene scene;

	Atlas::Lighting::DirectionalLight directionalLight;

	Atlas::Mesh::Mesh mesh;
	Atlas::Mesh::Mesh sphere;

	Atlas::Actor::StaticMeshActor meshActor;

	Atlas::Lighting::EnvironmentProbe probe;

	Atlas::Input::MouseHandler mouseHandler;
	Atlas::Input::KeyboardHandler keyboardHandler;

	std::vector<Atlas::Actor::MovableMeshActor> probeActors;

	bool renderUI = true;
	bool renderEnvProbe = true;
	bool spheresVisible = false;
	
	int32_t windowWidth;
	int32_t windowHeight;

	Atlas::Renderer::PathTracingRenderer pathTracingRenderer;

	ImguiWrapper imguiWrapper;

};

#endif