#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"
#include "tools/TerrainTool.h"
#include "Font.h"
#include "renderer/helper/GeometryHelper.h"

#include "libraries/stb/stb_image.h"

int main(int argc, char* argv[]) {

	Window* window = Engine::Init("../data/shader", "Blue Engine", WINDOWPOSITION_UNDEFINED,
		WINDOWPOSITION_UNDEFINED, 1280, 720, WINDOW_RESIZABLE | WINDOW_BORDERLESS);

	Engine::UnlockFramerate();

	GeometryHelper::GenerateSphereVertexArray(10, 10);

	Camera* camera = new Camera(47.0f, 2.0f, 1.0f, 4000.0f);
	camera->location = glm::vec3(30.0f, 25.0f, 0.0f);
	camera->rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

	Font* font = new Font("../data/roboto.ttf", 88, 5, 200);

	MasterRenderer* renderer = new MasterRenderer();

	int32_t textWidth, textHeight;
	font->ComputeDimensions("Loading...", 2.5f, &textWidth, &textHeight);

	int32_t x = 1280 / 2 - textWidth / 2;
	int32_t y = 720 / 2 - textHeight / 2;

	renderer->textRenderer->Render(window, font, "Loading...", x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.5f);

	window->Update();

	/*
	TerrainTool::GenerateHeightfieldLoDs("../data/terrain/heightfield.png", 9, 5, 16);

	Terrain* terrain = new Terrain(9, 5, 4, 1.0f, 300.0f);

	terrain->SetLoDDistance(4, 25.0f);
	terrain->SetLoDDistance(3, 50.0f);
	terrain->SetLoDDistance(2, 100.0f);
	terrain->SetLoDDistance(1, 200.0f);
	terrain->SetLoDDistance(0, 300.0f);
	*/

	Texture* texture = new Texture("../data/image.png");

	window->SetIcon(texture);

	RenderTarget* target = new RenderTarget(1920, 1080);

	Scene* scene = new Scene();

	Cubemap* cubemap = new Cubemap("../data/cubemap/right.png",
		"../data/cubemap/left.png",
		"../data/cubemap/top.png",
		"../data/cubemap/bottom.png",
		"../data/cubemap/front.png",
		"../data/cubemap/back.png");

	scene->sky->skybox = new Skybox(cubemap);

	scene->postProcessing->chromaticAberration = new ChromaticAberration(0.7f);

	// scene->Add(terrain);
	
	Mesh* mesh = new Mesh("../data/cube.dae");
	Mesh* sponzaMesh = new Mesh("../data/sponza/sponza.dae");

	Actor* actor = new Actor(mesh);
	Actor* sponza = new Actor(sponzaMesh);
	sponza->modelMatrix = scale(mat4(1.0f), vec3(0.05f));

	SceneNode* node = new SceneNode();

	Light* globalLight = new Light(DIRECTIONAL_LIGHT);
	globalLight->direction = vec3(0.0f, -1.0f, 0.5f);
	globalLight->diffuseColor = vec3(253, 194, 109) / 255.0f * 3.0f;
	globalLight->ambient = 0.05f;
	globalLight->AddShadow(new Shadow(125.0f, 0.004f, 2048, 3, 0.7f), camera);
	globalLight->AddVolumetric(new Volumetric(target->width / 2, target->height / 2, 20));

	node->Add(actor);
	scene->rootNode->Add(node);

	scene->Add(sponza);

	scene->Add(globalLight);

	node->transformationMatrix = translate(vec3(0.0f, 1.0f, 5.0f));

	// We create the controller handler
	MouseHandler* mouseHandler = CreateMouseHandler(camera, 1.5f, 0.25f);
	mouseHandler->lock = true;
	KeyboardHandler* keyboardHandler = CreateKeyboardHandler(camera,  7.0f, 0.3f);

	// For now we will leave the main loop here until we implement a more advanced event system
	// Our event structure
	SDL_Event event;
	bool quit = false;

	// We need the time passed per frame in the rendering loop
	unsigned int time = 0;

	// Handle events and rendering here
	while (!quit) {

		unsigned int deltatime = SDL_GetTicks() - time;
		time = SDL_GetTicks();

		// Poll all the events
		while (SDL_PollEvent(&event)) {

			if (event.type == SDL_QUIT) {

				// If the SDL event is telling us to quit we should do it
				quit = true;

			}
			else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {

				// If the user presses escape we also want to quit
				quit = true;

			}
			else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {

				// If the user presses escape we also want to quit
				quit = true;

			}
			else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {

				mouseHandler->lock = false;

			}
			else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {

				mouseHandler->lock = true;

			}

		}

		CalculateMouseHandler(mouseHandler, camera, deltatime);
		CalculateKeyboardHandler(keyboardHandler, camera, deltatime);

		camera->UpdateView();
		camera->UpdateProjection();

		/*
		terrain->Update(camera);

		for (TerrainStorageCell* cell : terrain->storage->requestedCells) {
			int32_t width, height, channels;

			string heightField("../data/terrain/LoD");
			heightField += to_string(cell->LoD) + "/height" + to_string(cell->x) + "-" + to_string(cell->y) + ".png";
			uint8_t* data = stbi_load(heightField.c_str(), &width, &height, &channels, 1);
			cell->heightField = new Texture(GL_UNSIGNED_BYTE, width, height, GL_R8, -0.4f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			cell->heightField->SetData(data);

			heightField = "../data/terrain/LoD";
			heightField += to_string(cell->LoD) + "/normal" + to_string(cell->x) + "-" + to_string(cell->y) + ".png";
			data = stbi_load(heightField.c_str(), &width, &height, &channels, 3);
			cell->normalMap = new Texture(GL_UNSIGNED_BYTE, width, height, GL_RGB8, -0.4f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			cell->normalMap->SetData(data);
			
		}

		terrain->storage->requestedCells.clear();
		*/

		scene->rootNode->transformationMatrix = glm::rotate((float)time / 1000.0f, vec3(0.0f, 1.0f, 0.0f));
		actor->modelMatrix = glm::rotate((float)time / 500.0f, vec3(0.0f, 1.0f, 0.0f));

		scene->Update();

		globalLight->shadow->Update(camera);

		renderer->RenderScene(window, target, camera, scene);

		renderer->textRenderer->RenderOutlined(window, font, "gHello World!", 0, 0, vec4(1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f), 2.0f, 2.5f);

		window->Update();

	}

	sponzaMesh->DeleteContent();
	delete sponzaMesh;

	return 0;

}