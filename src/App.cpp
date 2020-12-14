#include "App.h"

#ifndef AE_OS_ANDROID
std::string Atlas::EngineInstance::assetDirectory = "../../data";
#else
std::string Atlas::EngineInstance::assetDirectory = "data";
#endif
std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

	UnlockFramerate();

#ifdef AE_OS_ANDROID
	window.SetSize(GetScreenSize().x, GetScreenSize().y);
#endif

	viewport = Atlas::Viewport(0, 0, window.GetWidth(), window.GetHeight());

	auto icon = Atlas::Texture::Texture2D("icon.png");
	window.SetIcon(&icon);
	window.Update();

	font = Atlas::Font("roboto.ttf", 44, 10);

	DisplayLoadingScreen();

	camera = Atlas::Camera(45.0f, 2.0f, 1.0f, 400.0f,
		vec3(30.0f, 25.0f, 0.0f), vec2(-3.14f / 2.0f, 0.0f));
	//camera.exposure = 5.0f;

	scene = Atlas::Scene::Scene(vec3(-2048000.0f), vec3(2048000.0f));

#ifdef AE_OS_ANDROID
	touchHandler = Atlas::Input::TouchHandler(&camera, 1.5f, 7.0f, 6.0f);
#else
	mouseHandler = Atlas::Input::MouseHandler(&camera, 1.5f, 6.0f);
	keyboardHandler = Atlas::Input::KeyboardHandler(&camera, 7.0f, 6.0f);
	controllerHandler = Atlas::Input::ControllerHandler(&camera, 1.5f, 7.0f, 60.0f, 5000.0f);
#endif

	Atlas::Events::EventManager::ControllerDeviceEventDelegate.Subscribe(
		[this](Atlas::Events::ControllerDeviceEvent event) {
		if (event.type == AE_CONTROLLER_ADDED) {
			useControllerHandler = true;
		}
		else if (event.type == AE_CONTROLLER_REMOVED) {
			useControllerHandler = false;
		}
	});

	Atlas::Events::EventManager::KeyboardEventDelegate.Subscribe(
		[this](Atlas::Events::KeyboardEvent event) {
		if (event.keycode == AE_KEY_ESCAPE) {
			Exit();
		}
	});

	// Load the model
	/*
	mesh = Atlas::Mesh::Mesh("CornellBox/CornellBox-Original.obj", true);

	// Set ideal position for cornell box
	camera.location = glm::vec3(0.0f, 1.0f, 4.0f);
	camera.direction = glm::vec3(0.0f, 0.0f, -1.0f);
	*/
	
	mesh = Atlas::Mesh::Mesh("sponza/sponza.obj", true);

	// Set ideal position for cornell box
	camera.location = glm::vec3(-800.0f, 600.0f, 0.0f);
	camera.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
	
	/*
	mesh = Atlas::Mesh::Mesh("living room/living_room_night.obj", true);

	// Set ideal position for living room
	camera.location = glm::vec3(-1.5f, 1.5f, 5.5f);
	camera.direction = glm::normalize(glm::vec3(0.5f, -0.2f, -1.0f));
	*/
	/*
	mesh = Atlas::Mesh::Mesh("bistro/mesh/exterior.obj", true, 1024);

	// Set ideal position for living room
	camera.location = glm::vec3(-82.046f, 300.702f, 62.696f) + glm::normalize(glm::vec3(-0.5f, 0.0f, -1.0f)) * 1200.0f;
	camera.direction = -glm::normalize(glm::vec3(-0.5f, 0.1f, -1.0f));
	*/
	/*
	mesh = Atlas::Mesh::Mesh("emissive mesh/emissive_mesh.obj", true);

	// Set ideal position for cornell box
	camera.location = glm::vec3(0.0f, 10.0f, 20.0f);
	camera.direction = glm::normalize(glm::vec3(0.0f, -.5f, -1.0f));
	*/
	mesh.invertUVs = true;

	actor = Atlas::Actor::StaticMeshActor(&mesh, glm::mat4(1.0f));

	scene.Add(&actor);

#ifndef AE_OS_ANDROID
	rayTraceTarget = Atlas::Renderer::RayTracerRenderTarget(1280, 720);
#else
	rayTraceTarget = Atlas::Renderer::RayTracerRenderTarget(1280, 720);
#endif

}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {

	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 right = glm::normalize(glm::cross(up, -camera.direction));
	up = glm::normalize(glm::cross(-camera.direction, right));

	camera.up = up;
	camera.right = right;

	camera.viewMatrix = lookAt(camera.location, camera.location + camera.direction, up);
	camera.invViewMatrix = glm::inverse(camera.viewMatrix);

	camera.UpdateProjection();

	scene.Update(&camera, deltaTime);

}

void App::Render(float deltaTime) {

	static int32_t count = 0;
	auto sampleCount = rayTracingRenderer.GetSampleCount();
	if (sampleCount % 1000 == 1 &&
		count != sampleCount) {
		count = sampleCount;
		rayTraceTarget.texture.Save<uint8_t>("emissive_128_" + std::to_string(sampleCount - 1) + "spp2");
	}
	
	window.Clear();
	
    viewport.Set(0, 0, rayTraceTarget.GetWidth(), rayTraceTarget.GetHeight());

#ifndef AE_OS_ANDROID
	rayTracingRenderer.Render(&viewport, &rayTraceTarget, ivec2(1, 1), &camera, &scene);
#else
	rayTracingRenderer.Render(&viewport, &rayTraceTexture, &inAccumTexture,
        &outAccumTexture, ivec2(16, 9), &camera, &scene);
#endif

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
		GL_SHADER_STORAGE_BARRIER_BIT);

    viewport.Set(0, 0, window.GetWidth(), window.GetHeight());

	masterRenderer.RenderTexture(&viewport, &rayTraceTarget.texture, 0.0f, 0.0f,
		(float)viewport.width, (float)viewport.height);	

	float averageFramerate = Atlas::Clock::GetAverage();

	std::string out = "Average " + std::to_string(averageFramerate) + " ms  Currently " + std::to_string(deltaTime) + " ms"
		+ " " + std::to_string(rayTracingRenderer.GetSampleCount()) + " samples";

	masterRenderer.textRenderer.Render(&viewport, &font, out, 0, 0, vec4(1.0f, 0.0f, 0.0f, 1.0f), 5.0f / 10.0f);

}

void App::DisplayLoadingScreen() {

	float textWidth, textHeight;
	font.ComputeDimensions("Loading...", 2.5f, &textWidth, &textHeight);

	window.Clear();

	auto windowSize = window.GetDrawableSize();

	float x = windowSize.x / 2 - textWidth / 2;
	float y = windowSize.y / 2 - textHeight / 2;

	masterRenderer.textRenderer.Render(&viewport, &font, "Loading...", x, y, vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.5f);

	window.Update();

}

Atlas::EngineInstance* GetEngineInstance() {

	return new App();

}