#ifndef MAIN_H
#define MAIN_H

#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"
#include "input/Controller.h"

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

	void ControllerDeviceEventHandler(EngineControllerDeviceEvent event);

	void MouseButtonEventHandler(EngineMouseButtonEvent event);

	void TextInputEventHandler(EngineTextInputEvent event);

	Window* window;

	MouseHandler* mouseHandler;
	KeyboardHandler* keyboardHandler;
	ControllerHandler* controllerHandler;

	RenderTarget* renderTarget;
	MasterRenderer* masterRenderer;

	Font* font;

	Camera* camera;
	Scene* scene;

	Cubemap* skybox;
	Terrain* terrain;
	Mesh* sponzaMesh;
	Mesh* treeMesh;
	Mesh* cubeMesh;

	Actor* cubeActor;
	Actor* treeActor;
	Actor* sponzaActor;

	Texture2D* terrainDiffuseMap;
	Texture2D* terrainDisplacementMap;

	Texture2D* smileyTexture;

	uint32_t renderingStart;
	uint32_t frameCount;

	string outString;

	bool quit;
	bool useControllerHandler;

};

#endif