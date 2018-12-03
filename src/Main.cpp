#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"
#include "tools/TerrainTool.h"
#include "Font.h"
#include "renderer/helper/GeometryHelper.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"

static bool quit = false;

void QuitEventHandler() {

	quit = true;

}

int main(int argc, char* argv[]) {

	Window* window = Engine::Init("../data/shader", "Blue Engine", WINDOWPOSITION_UNDEFINED,
		WINDOWPOSITION_UNDEFINED, 1280, 720, WINDOW_RESIZABLE);

	Engine::UnlockFramerate();

	Camera* camera = new Camera(47.0f, 2.0f, 1.0f, 4000.0f);
	camera->location = glm::vec3(30.0f, 25.0f, 0.0f);
	camera->rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

	Font* font = new Font("../data/roboto.ttf", 44, 5, 200);

	MasterRenderer* renderer = new MasterRenderer();

	float textWidth, textHeight;
	font->ComputeDimensions("Loading...", 2.5f, &textWidth, &textHeight);

	float lineHeight = (float)font->lineHeight * 2.5f;

	float x = 1280 / 2 - textWidth / 2;
	float y = 720 / 2 - lineHeight / 2;

	renderer->textRenderer->Render(window, font, "Loading...", x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.5f);

	window->Update();
	
	// TerrainTool::GenerateHeightfieldLoDs("../data/terrain/heightfield.png", 256, 1, 16);

	Terrain* terrain = new Terrain(256, 1, 4, 1.0f, 300.0f);
	terrain->SetTessellationFunction(400.0f, 2.0f, 0.0f);
	terrain->SetDisplacementDistance(15.0f);

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

	scene->Add(terrain);
	
	Mesh* mesh = new Mesh("../data/cube.dae");
	Mesh* sponzaMesh = new Mesh("../data/sponza/sponza.dae");
	Mesh* treeMesh = new Mesh("../data/tree.dae");

	Actor* actor = new Actor(mesh);
	Actor* tree = new Actor(treeMesh);
	tree->modelMatrix = scale(mat4(1.0f), vec3(3.0f));
	Actor* sponza = new Actor(sponzaMesh);
	sponza->modelMatrix = scale(mat4(1.0f), vec3(0.05f));

	SceneNode* node = new SceneNode();

	Light* globalLight = new Light(DIRECTIONAL_LIGHT);
	globalLight->direction = vec3(0.0f, -1.0f, 0.2f);
	//globalLight->diffuseColor = vec3(253, 194, 109) / 255.0f;
	globalLight->ambient = 0.05f;
	globalLight->AddShadow(new Shadow(125.0f, 0.008f, 1024, 3, 0.7f), camera);
	//globalLight->AddVolumetric(new Volumetric(target->width / 2, target->height / 2, 20, -0.5f));

	Light* pointLight1 = new Light(POINT_LIGHT);
	pointLight1->location = vec3(24.35f, 6.5f, 7.1f);
	pointLight1->diffuseColor = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight1->ambient = 0.1f;

	Light* pointLight2 = new Light(POINT_LIGHT);
	pointLight2->location = vec3(24.35f, 6.5f, -11.0f);
	pointLight2->diffuseColor = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight2->ambient = 0.1f;

	Light* pointLight3 = new Light(POINT_LIGHT);
	pointLight3->location = vec3(-31.0f, 6.5f, 7.1f);
	pointLight3->diffuseColor = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight3->ambient = 0.1f;

	Light* pointLight4 = new Light(POINT_LIGHT);
	pointLight4->location = vec3(-31.0f, 6.5f, -11.0f);
	pointLight4->diffuseColor = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight4->ambient = 0.1f;

	Light* pointLight5 = new Light(POINT_LIGHT);
	pointLight5->location = vec3(0.0f, 2.0f, 0.0f);
	pointLight5->diffuseColor = vec3(1.0f);
	pointLight5->ambient = 0.1f;

	node->Add(actor);
	scene->rootNode->Add(node);

	scene->Add(sponza);
	scene->Add(tree);

	scene->Add(globalLight);

    scene->Add(pointLight1);
    scene->Add(pointLight2);
    scene->Add(pointLight3);
    scene->Add(pointLight4);
    scene->Add(pointLight5);

	node->transformationMatrix = translate(vec3(0.0f, 1.0f, 5.0f));

	Texture* terrainDiffuseMap = new Texture("../data/terrain/Ground_17_DIF.jpg");
	Texture* terrainDisplacementMap = new Texture("../data/terrain/Ground_17_DISP.jpg");

	// We create the controller handler
	MouseHandler* mouseHandler = new MouseHandler(camera, 1.5f, 0.015f);
	KeyboardHandler* keyboardHandler = CreateKeyboardHandler(camera,  7.0f, 0.3f);

	SystemEventHandler::quitEventDelegate.Subscribe(QuitEventHandler);

	// We need the time passed per frame in the rendering loop
	unsigned int time = 0;

	// Handle events and rendering here
	while (!quit) {

		unsigned int deltatime = SDL_GetTicks() - time;
		time = SDL_GetTicks();

		Engine::Update();		

		mouseHandler->Update(camera, deltatime);
		CalculateKeyboardHandler(keyboardHandler, camera, deltatime);

		camera->UpdateView();
		camera->UpdateProjection();

		
		terrain->Update(camera);

		for (TerrainStorageCell* cell : terrain->storage->requestedCells) {
			int32_t width, height, channels;

			string heightField("../data/terrain/LoD");
			heightField += to_string(cell->LoD) + "/height" + to_string(cell->x) + "-" + to_string(cell->y) + ".png";
			uint8_t* data = stbi_load(heightField.c_str(), &width, &height, &channels, 1);
			cell->heightField = new Texture(GL_UNSIGNED_BYTE, width, height, GL_R8, -0.4f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			auto dataVector = vector<uint8_t>(width * height);
			dataVector.assign(data, data + width * height);
			cell->heightField->SetData(dataVector);

			heightField = "../data/terrain/LoD";
			heightField += to_string(cell->LoD) + "/normal" + to_string(cell->x) + "-" + to_string(cell->y) + ".png";
			data = stbi_load(heightField.c_str(), &width, &height, &channels, 3);
			cell->normalMap = new Texture(GL_UNSIGNED_BYTE, width, height, GL_RGB8, -0.4f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
			dataVector.assign(data, data + width * height * 3);
			cell->normalMap->SetData(dataVector);

			cell->diffuseMap = terrainDiffuseMap;
			cell->displacementMap = terrainDisplacementMap;
			
		}

		terrain->storage->requestedCells.clear();

		// EngineLog("%.3f,.%3f", camera->location.x, camera->location.z);
		
		scene->rootNode->transformationMatrix = glm::rotate((float)time / 1000.0f, vec3(0.0f, 1.0f, 0.0f));
		actor->modelMatrix = glm::rotate((float)time / 500.0f, vec3(0.0f, 1.0f, 0.0f));

		scene->Update(camera);

		globalLight->shadow->Update(camera);

		renderer->RenderScene(window, target, camera, scene);

		renderer->textRenderer->Render(window, font, "gHello World!", 0, 0, vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.5f / 5.0f);

		window->Update();

	}

	sponzaMesh->DeleteContent();
	delete sponzaMesh;

	return 0;

}