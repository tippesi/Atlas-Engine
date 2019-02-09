#ifndef AE_MAIN_H
#define AE_MAIN_H

#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"
#include "input/Controller.h"
#include "input/Touch.h"

class Main {

public:
	Main(int argc, char* argv[]);

private:
	void Update(uint32_t deltaTime);

	void Render(uint32_t deltaTime);

	void Stream();

	void Load();

	void DisplayLoadingScreen();

	void SceneSetUp();

	void QuitEventHandler();

	void ControllerDeviceEventHandler(Atlas::Events::ControllerDeviceEvent event);

	void MouseButtonEventHandler(Atlas::Events::MouseButtonEvent event);

	Atlas::Window* window;

	Atlas::Input::MouseHandler* mouseHandler;
	Atlas::Input::KeyboardHandler* keyboardHandler;
	Atlas::Input::ControllerHandler* controllerHandler;
	Atlas::Input::TouchHandler* touchHandler;

	Atlas::RenderTarget* renderTarget;
	Atlas::Renderer::MasterRenderer* masterRenderer;

	Atlas::Font* font;

	Atlas::Camera* camera;
	Atlas::Scene* scene;

	Atlas::Texture::Cubemap* skybox;
	Atlas::Terrain::Terrain* terrain;

	Atlas::Mesh::Mesh* sponzaMesh;
	Atlas::Mesh::Mesh* treeMesh;
	Atlas::Mesh::Mesh* cubeMesh;

	Atlas::Mesh::MeshActor* cubeActor;
	Atlas::Mesh::MeshActor* treeActor;
	Atlas::Mesh::MeshActor* sponzaActor;

	Atlas::Texture::Texture2D* terrainDiffuseMap;
	Atlas::Texture::Texture2D* terrainDisplacementMap;

	Atlas::Texture::Texture2D* smileyTexture;

	Atlas::Lighting::DirectionalLight* directionalLight;

	uint32_t renderingStart;
	uint32_t frameCount;

	bool quit;
	bool useControllerHandler;

};

#endif