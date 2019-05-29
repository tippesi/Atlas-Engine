#ifndef AE_APP_H
#define AE_APP_H

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>

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
	Atlas::RenderTarget* renderTarget;
	Atlas::Viewport viewport;

	Atlas::Font font;

	Atlas::Camera camera;

	Atlas::Scene::Scene scene;

	Atlas::Lighting::DirectionalLight directionalLight;
	Atlas::Lighting::PointLight pointLight0;
	Atlas::Lighting::PointLight pointLight1;
	Atlas::Lighting::PointLight pointLight2;
	Atlas::Lighting::PointLight pointLight3;

	Atlas::Mesh::Mesh sponzaMesh;
	Atlas::Mesh::Mesh treeMesh;
	Atlas::Mesh::Mesh cubeMesh;

	Atlas::Actor::MovableMeshActor cubeActor;
	Atlas::Actor::StaticMeshActor treeActor;
	Atlas::Actor::StaticMeshActor sponzaActor;

	Atlas::Audio::AudioData* audioData;
	Atlas::Audio::AudioStream* audioStream;

	Atlas::Input::MouseHandler mouseHandler;
	Atlas::Input::KeyboardHandler keyboardHandler;
	Atlas::Input::ControllerHandler controllerHandler;
	Atlas::Input::TouchHandler touchHandler;

	bool useControllerHandler;

};

#endif