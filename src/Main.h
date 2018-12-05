#ifndef MAIN_H
#define MAIN_H

#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"

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

	void SystemQuitEventHandler();

	Window* window;

	MouseHandler* mouseHandler;
	KeyboardHandler* keyboardHandler;

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

	Texture* terrainDiffuseMap;
	Texture* terrainDisplacementMap;

	bool quit;

};

#endif