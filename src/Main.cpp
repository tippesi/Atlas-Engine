#include "Main.h"

#include "tools/TerrainTool.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"

Main::Main(int argc, char* argv[]) {

	quit = false;

	window = Engine::Init("../data/shader", "Blue Engine", WINDOWPOSITION_UNDEFINED,
		WINDOWPOSITION_UNDEFINED, 1280, 720, WINDOW_RESIZABLE);

	Engine::UnlockFramerate();

	// Register quit event
	auto quitEventHandler = std::bind(&Main::SystemQuitEventHandler, this);
	SystemEventHandler::quitEventDelegate.Subscribe(quitEventHandler);

	camera = new Camera(47.0f, 2.0f, 1.0f, 4000.0f);
	camera->location = glm::vec3(30.0f, 25.0f, 0.0f);
	camera->rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

	mouseHandler = new MouseHandler(camera, 1.5f, 0.015f);
	keyboardHandler = CreateKeyboardHandler(camera, 7.0f, 0.3f);

	masterRenderer = new MasterRenderer();
	renderTarget = new RenderTarget(1920, 1080);

	Load();
	SceneSetUp();

	uint32_t time = 0;

	while (!quit) {

		uint32_t deltaTime = SDL_GetTicks() - time;
		time = SDL_GetTicks();

		Engine::Update();

		Update(deltaTime);
		Stream(); // Should be done in another thread
		Render(deltaTime);

		window->Update();

	}	

}

void Main::Update(uint32_t deltaTime) {

	mouseHandler->Update(camera, deltaTime);
	CalculateKeyboardHandler(keyboardHandler, camera, deltaTime);

	camera->UpdateView();
	camera->UpdateProjection();

	terrain->Update(camera);

	scene->rootNode->transformationMatrix = glm::rotate((float)SDL_GetTicks() / 1000.0f, vec3(0.0f, 1.0f, 0.0f));
	cubeActor->modelMatrix = glm::rotate((float)SDL_GetTicks() / 500.0f, vec3(0.0f, 1.0f, 0.0f));

	scene->Update(camera);

	directionalLight->shadow->Update(camera);

}

void Main::Render(uint32_t deltaTime) {

	masterRenderer->RenderScene(window, renderTarget, camera, scene);

	masterRenderer->textRenderer->Render(window, font, "gHello World!", 0, 0, vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.5f / 5.0f);

}

void Main::Stream() {

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

}

void Main::Load() {

	font = new Font("../data/roboto.ttf", 88, 5, 200);

	DisplayLoadingScreen();

	skybox = new Cubemap("../data/cubemap/right.png",
		"../data/cubemap/left.png",
		"../data/cubemap/top.png",
		"../data/cubemap/bottom.png",
		"../data/cubemap/front.png",
		"../data/cubemap/back.png");

	cubeMesh = new Mesh("../data/cube.dae");
	sponzaMesh = new Mesh("../data/sponza/sponza.dae");
	treeMesh = new Mesh("../data/tree.dae");

	terrainDiffuseMap = new Texture("../data/terrain/Ground_17_DIF.jpg");
	terrainDisplacementMap = new Texture("../data/terrain/Ground_17_DISP.jpg");

}

void Main::DisplayLoadingScreen() {

	float textWidth, textHeight;
	font->ComputeDimensions("Loading...", 2.5f, &textWidth, &textHeight);

	float x = 1280 / 2 - textWidth / 2;
	float y = 720 / 2 - textHeight / 2;

	masterRenderer->textRenderer->Render(window, font, "Loading...", x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.5f);

	window->Update();

}

void Main::SceneSetUp() {

	scene = new Scene();

	terrain = new Terrain(256, 1, 4, 1.0f, 300.0f);
	terrain->SetTessellationFunction(400.0f, 2.0f, 0.0f);
	terrain->SetDisplacementDistance(15.0f);

	scene->Add(terrain);

	SceneNode* node = new SceneNode();
	node->transformationMatrix = translate(vec3(0.0f, 1.0f, 5.0f));
	scene->rootNode->Add(node);

	cubeActor = new Actor(cubeMesh);
	treeActor = new Actor(treeMesh);
	treeActor->modelMatrix = scale(mat4(1.0f), vec3(3.0f));
	sponzaActor = new Actor(sponzaMesh);
	sponzaActor->modelMatrix = scale(mat4(1.0f), vec3(0.05f));

	directionalLight = new DirectionalLight(MOVABLE_LIGHT);
	directionalLight->direction = vec3(0.0f, -1.0f, 0.2f);
	// directionalLight->color = vec3(253, 194, 109) / 255.0f;
	directionalLight->ambient = 0.05f;
	directionalLight->AddShadow(new Shadow(125.0f, 0.008f, 1024, 3, 0.7f), camera);
	// directionalLight->AddVolumetric(new Volumetric(target->width / 2, target->height / 2, 20, -0.5f));

	PointLight* pointLight1 = new PointLight(STATIONARY_LIGHT);
	pointLight1->location = vec3(24.35f, 6.5f, 7.1f);
	pointLight1->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight1->ambient = 0.1f;

	PointLight* pointLight2 = new PointLight(STATIONARY_LIGHT);
	pointLight2->location = vec3(24.35f, 6.5f, -11.0f);
	pointLight2->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight2->ambient = 0.1f;

	PointLight* pointLight3 = new PointLight(STATIONARY_LIGHT);
	pointLight3->location = vec3(-31.0f, 6.5f, 7.1f);
	pointLight3->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight3->ambient = 0.1f;

	PointLight* pointLight4 = new PointLight(STATIONARY_LIGHT);
	pointLight4->location = vec3(-31.0f, 6.5f, -11.0f);
	pointLight4->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight4->ambient = 0.1f;

	PointLight* pointLight5 = new PointLight(STATIONARY_LIGHT);
	pointLight5->location = vec3(0.0f, 2.0f, 0.0f);
	pointLight5->color = vec3(1.0f);
	pointLight5->ambient = 0.1f;

	node->Add(cubeActor);
	scene->Add(sponzaActor);
	scene->Add(treeActor);

	scene->Add(directionalLight);

	scene->Add(pointLight1);
	scene->Add(pointLight2);
	scene->Add(pointLight3);
	scene->Add(pointLight4);
	scene->Add(pointLight5);

}

void Main::SystemQuitEventHandler() {

	quit = true;

}


int main(int argc, char* argv[]) {

	
	// TerrainTool::GenerateHeightfieldLoDs("../data/terrain/heightfield.png", 256, 1, 16);

	Main mainClass(argc, argv);

	return 0;

}