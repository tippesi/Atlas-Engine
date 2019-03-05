#include "Main.h"

#include "tools/TerrainTool.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"

using namespace Atlas;

Main::Main(int argc, char* argv[]) {

	quit = false;
	useControllerHandler = false;

	int32_t width, height, flags;
	Engine::GetScreenSize(&width, &height);

	std::string assetDirectory = "data";

#ifndef AE_OS_ANDROID
	assetDirectory = "../data";
	width = 1280;
	height = 720;
	flags = AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI;
#else
	flags = AE_WINDOW_FULLSCREEN;
#endif

	window = Engine::Init(assetDirectory, "shader", "Atlas Engine",AE_WINDOWPOSITION_UNDEFINED,
			AE_WINDOWPOSITION_UNDEFINED, width, height, flags);

	Engine::UnlockFramerate();

	auto icon = Texture::Texture2D("icon.png");
    window->SetIcon(&icon);
	window->Update();

	// Register quit event
	auto quitEventHandler = std::bind(&Main::QuitEventHandler, this);
    Atlas::Events::EventManager::QuitEventDelegate.Subscribe(quitEventHandler);

	auto controllerDeviceEventHandler = std::bind(&Main::ControllerDeviceEventHandler, this, std::placeholders::_1);
    Atlas::Events::EventManager::ControllerDeviceEventDelegate.Subscribe(controllerDeviceEventHandler);

	camera = new Camera(47.0f, 2.0f, .25f, 400.0f);
	camera->location = vec3(30.0f, 25.0f, 0.0f);
	camera->rotation = vec2(-3.14f / 2.0f, 0.0f);

#ifdef AE_OS_ANDROID
	touchHandler = new Atlas::Input::TouchHandler(camera, 1.5f, 7.0f, 0.2f);
#else
	mouseHandler = new Atlas::Input::MouseHandler(camera, 1.5f, 6.0f);
	keyboardHandler = new Atlas::Input::KeyboardHandler(camera, 7.0f, 6.0f);
	controllerHandler = new Atlas::Input::ControllerHandler(camera, 1.5f, 7.0f, 0.2f, 5000.0f);
#endif

	masterRenderer = new Renderer::MasterRenderer();

#ifndef AE_OS_ANDROID
	renderTarget = new Atlas::RenderTarget(1920, 1080);
#else
    renderTarget = new RenderTarget(1280, 720);
    Texture::Texture::SetAnisotropyLevel(8);
#endif
	Load();

	SceneSetUp();

	float time = Engine::GetClock();

	renderingStart = Engine::GetClock();
	frameCount = 1;

	while (!quit) {

		float deltaTime = Engine::GetClock() - time;
		time = Engine::GetClock();

		Engine::Update();

		Update(deltaTime);
		Stream(); // Should be done in another thread
		Render(deltaTime);

		window->Update();

		frameCount++;

	}

}

void Main::Update(float deltaTime) {

#ifdef AE_OS_ANDROID
	touchHandler->Update(camera, deltaTime);
#else
	if (!useControllerHandler) {
		mouseHandler->Update(camera, deltaTime);
		keyboardHandler->Update(camera, deltaTime);
	}
	else {
		controllerHandler->Update(camera, deltaTime);
	}
#endif

	camera->UpdateView();
	camera->UpdateProjection();

	scene->Update(camera, deltaTime);

	masterRenderer->Update();

}

void Main::Render(float deltaTime) {

	masterRenderer->RenderScene(window, renderTarget, camera, scene);

	float averageFramerate = (Engine::GetClock() - renderingStart) * 1000.0f / (float)frameCount;

	std::string out = "Average " + std::to_string(averageFramerate) + " ms  Currently " + std::to_string(deltaTime) + " ms";

	masterRenderer->textRenderer.Render(window, font, out, 0, 0, vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.5f / 10.0f, true);

}

void Main::Stream() {



}

void Main::Load() {

	font = new Font("roboto.ttf", 88, 5, 200);

	DisplayLoadingScreen();

	skybox = new Texture::Cubemap("cubemap/right.png",
		"cubemap/left.png",
		"cubemap/top.png",
		"cubemap/bottom.png",
		"cubemap/front.png",
		"cubemap/back.png");

	cubeMesh = new Mesh::Mesh("cube.dae");
	treeMesh = new Mesh::Mesh("tree.dae");
	treeMesh->cullBackFaces = false;
	sponzaMesh = new Mesh::Mesh("sponza/sponza.dae");

}

void Main::DisplayLoadingScreen() {

	float textWidth, textHeight;
	font->ComputeDimensions("Lädt...", 2.5f, &textWidth, &textHeight);

    window->Clear();

    int32_t width, height;
    Engine::GetScreenSize(&width, &height);

#ifndef AE_OS_ANDROID
    width = 1280;
	height = 720;
#endif

	float x = width / 2 - textWidth / 2;
	float y = height / 2 - textHeight / 2;

	masterRenderer->textRenderer.Render(window, font, "Lädt...", x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.5f);

	window->Update();

}

void Main::SceneSetUp() {

	scene = new Atlas::Scene::Scene();

	auto node = new Atlas::Scene::SceneNode();
	node->SetMatrix(translate(vec3(0.0f, 1.0f, 5.0f)));
	scene->Add(node);
	scene->sky.skybox = new Lighting::Skybox(skybox);

	cubeActor = new Actor::MeshActor(cubeMesh);
	treeActor = new Actor::MeshActor(treeMesh);
	treeActor->SetMatrix(scale(mat4(1.0f), vec3(3.0f)));
	sponzaActor = new Actor::MeshActor(sponzaMesh);
	sponzaActor->SetMatrix(scale(mat4(1.0f), vec3(0.05f)));

	directionalLight = new Lighting::DirectionalLight(AE_STATIONARY_LIGHT);
#ifdef AE_OS_ANDROID
	directionalLight->direction = vec3(0.0f, -1.0f, 0.5f);
    directionalLight->ambient = 0.005f;
#else
    directionalLight->direction = vec3(0.0f, -1.0f, 0.1f);
    directionalLight->ambient = 0.05f;
#endif
	directionalLight->color = vec3(253, 194, 109) / 255.0f;

	// Cascaded shadow mapping
	// directionalLight->AddShadow(300.0f, 0.01f, 1024, 4, 0.7f, camera);
	// Shadow mapping that is fixed to a point
    mat4 orthoProjection = glm::ortho(-100.0f, 100.0f, -70.0f, 120.0f, -120.0f, 120.0f);
#ifndef AE_OS_ANDROID
    directionalLight->AddShadow(200.0f, 0.01f, 4096, vec3(0.0f), orthoProjection);
	directionalLight->GetShadow()->sampleCount = 1;
	directionalLight->AddVolumetric(renderTarget->width / 2, renderTarget->height / 2, 20, -0.5f);
#else
    directionalLight->AddShadow(200.0f, 0.01f, 2048, vec3(0.0f), orthoProjection);
    directionalLight->GetShadow()->sampleCount = 8;
    directionalLight->AddVolumetric(renderTarget->width / 2, renderTarget->height / 2, 10, -0.5f);
#endif
	auto pointLight1 = new Lighting::PointLight(AE_STATIONARY_LIGHT);
	pointLight1->location = vec3(24.35f, 6.5f, 7.1f);
	pointLight1->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight1->AddShadow(0.0f, 512);

	auto pointLight2 = new Lighting::PointLight(AE_STATIONARY_LIGHT);
	pointLight2->location = vec3(24.35f, 6.5f, -11.0f);
	pointLight2->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight2->AddShadow(0.0f, 512);

	auto pointLight3 = new Lighting::PointLight(AE_STATIONARY_LIGHT);
	pointLight3->location = vec3(-31.0f, 6.5f, 7.1f);
	pointLight3->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight3->AddShadow(0.0f, 512);

	auto pointLight4 = new Lighting::PointLight(AE_STATIONARY_LIGHT);
	pointLight4->location = vec3(-31.0f, 6.5f, -11.0f);
	pointLight4->color = 2.0f * vec3(255.0f, 128.0f, 0.0f) / 255.0f;
	pointLight4->AddShadow(0.0f, 512);

	node->Add(cubeActor);
	scene->Add(sponzaActor);
	scene->Add(treeActor);

	scene->Add(directionalLight);

	scene->Add(pointLight1);
	scene->Add(pointLight2);
	scene->Add(pointLight3);
	scene->Add(pointLight4);
	
}

void Main::QuitEventHandler() {

	quit = true;

}

void Main::ControllerDeviceEventHandler(Atlas::Events::ControllerDeviceEvent event) {

	if (event.type == AE_CONTROLLER_ADDED) {
		useControllerHandler = true;
	}
	else if (event.type == AE_CONTROLLER_REMOVED) {
		useControllerHandler = false;
	}

}

int main(int argc, char* argv[]) {

	Main mainClass(argc, argv);

	return 0;

}