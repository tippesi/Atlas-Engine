#include "App.h"

#include <chrono>
#include <thread>

const std::string Atlas::EngineInstance::assetDirectory = "../../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

	UnlockFramerate();

	renderTarget = new Atlas::RenderTarget(1280 * 2, 720 * 2);

	auto icon = Atlas::Texture::Texture2D("icon.png");
	window.SetIcon(&icon);
	window.Update();

	font = Atlas::Font("font/roboto.ttf", 44, 10);

	camera = Atlas::Camera(47.0f, 2.0f, 1.0f, 400.0f,
		vec3(30.0f, 25.0f, 0.0f), vec2(-3.14f / 2.0f, 0.0f));

	scene = Atlas::Scene::Scene(vec3(-2048.0f), vec3(2048.0f));

	mouseHandler = Atlas::Input::MouseHandler(&camera, 1.5f, 6.0f);
	keyboardHandler = Atlas::Input::KeyboardHandler(&camera, 7.0f, 6.0f);

	Atlas::Events::EventManager::KeyboardEventDelegate.Subscribe(
		[this](Atlas::Events::KeyboardEvent event) {
			if (event.keycode == AE_KEY_ESCAPE) {
				Exit();
			}
			if (event.keycode == AE_KEY_F11 && event.state == AE_BUTTON_RELEASED) {
				renderUI = !renderUI;
			}
		});
	
	directionalLight = Atlas::Lighting::DirectionalLight(AE_MOVABLE_LIGHT);
	directionalLight.direction = vec3(0.0f, -1.0f, 1.0f);
	directionalLight.color = vec3(253, 194, 109) / 255.0f;

	// Cascaded shadow mapping
	directionalLight.AddShadow(150.0f, 1.1f, 1024, 5, 0.8f);
	directionalLight.AddVolumetric(10, 0.28f);

	scene.Add(&directionalLight);

	scene.ssao = new Atlas::Lighting::SSAO(32);

	scene.fog = new Atlas::Lighting::Fog();

	scene.fog->enable = true;
	scene.fog->density = 0.0002f;
	scene.fog->heightFalloff = 0.0284f;
	scene.fog->height = 0.0f;
	scene.fog->scatteringAnisotropy = 0.0f;

	LoadScene();

	rayTraceTarget = Atlas::Renderer::PathTracerRenderTarget(1280 * 2, 720 * 2);
	scene.postProcessing.taa = Atlas::PostProcessing::TAA(0.99f);
	// Use against TAA smoothing
	scene.postProcessing.sharpen.enable = true;
	scene.postProcessing.sharpen.factor = 0.15f;

	sphere = Atlas::Mesh::Mesh("sphere.dae", AE_MOVABLE_MESH);
	sphere.SetTransform(glm::scale(mat4(1.0f), vec3(0.09f)));
	sphere.data.materials[0].roughness = 1.0;
	sphere.data.materials[0].metalness = 0.0;
	sphere.data.materials[0].baseColor = vec3(1.0);

	scene.Update(&camera, 1.0f);
	scene.BuildRTStructures();
	
	auto volume = scene.irradianceVolume;
	volume->hysteresis = 0.99f;

	int32_t probeCount = volume->probeCount.x * volume->probeCount.y *
		volume->probeCount.z;

	for (int32_t j = 0; j < probeCount; j++) {
		auto actor = new Atlas::Actor::MovableMeshActor(&sphere);
		actor->visible = false;
		sphereActors.push_back(actor);

		scene.Add(actor);
	}

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	imguiWrapper = ImguiWrapper(&window, &context);

	io.Fonts->AddFontFromFileTTF(
		Atlas::Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(),
		20.0f);

	ImGui::StyleColorsDark();	

}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {

	ImGuiIO& io = ImGui::GetIO();

	imguiWrapper.Update(&window, deltaTime);

	if (io.WantCaptureMouse) {
		mouseHandler.lock = true;
	}
	else {
		mouseHandler.lock = false;
	}

	mouseHandler.Update(&camera, deltaTime);
	keyboardHandler.Update(&camera, deltaTime);

	camera.UpdateView();
	camera.UpdateProjection();

	{
		static bool renderSpheres = false;
		if (renderSpheres != spheresVisible) {
			renderSpheres = spheresVisible;
			for (auto actor : sphereActors)
			{
				actor->visible = renderSpheres;
			}
		}
		if (renderSpheres) {
			auto volume = scene.irradianceVolume;

			int32_t probeCount = volume->probeCount.x * volume->probeCount.y *
				volume->probeCount.z;

			vec3 volumeSize = volume->aabb.max - volume->aabb.min;
			vec3 cellSize = volumeSize / vec3(volume->probeCount - ivec3(1));
			vec3 halfCell = cellSize * 0.5f;

			for (int32_t j = 0; j < probeCount; j++) {
				int32_t off = j;
				int32_t z = off / (volume->probeCount.x * volume->probeCount.y);
				off -= (z * volume->probeCount.x * volume->probeCount.y);
				int32_t y = off / volume->probeCount.x;
				int32_t x = off % volume->probeCount.x;

				ivec3 offset = ivec3(x, y, z);
				vec3 pos = scene.irradianceVolume->GetProbeLocation(offset);

				sphereActors[j]->SetMatrix(glm::translate(pos));
			}
		}
	}

	scene.Update(&camera, deltaTime);

}

void App::Render(float deltaTime) {

	static bool animateLight = false;
	static bool pathTrace = false;
	static bool showAo = false;
	static bool slowMode = false;
	
	window.Clear();

	if (animateLight) directionalLight.direction = vec3(0.0f, -1.0f, sin(Atlas::Clock::Get() / 10.0f));	
	
	if (pathTrace) {
		viewport.Set(0, 0, rayTraceTarget.GetWidth(), rayTraceTarget.GetHeight());
		rayTracingRenderer.Render(&viewport, &rayTraceTarget, ivec2(1, 1), &camera, &scene);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
			GL_SHADER_STORAGE_BARRIER_BIT);

		viewport.Set(0, 0, window.GetWidth(), window.GetHeight());
		masterRenderer.RenderTexture(&viewport, &rayTraceTarget.texture, 0.0f, 0.0f,
			(float)viewport.width, (float)viewport.height);
	}
	else {
		viewport.Set(0, 0, window.GetWidth(), window.GetHeight());
		masterRenderer.RenderScene(&viewport, renderTarget, &camera, &scene);

		if (showAo) {
			masterRenderer.RenderTexture(&viewport, &renderTarget->ssaoTexture, 0.0f, 0.0f,
				viewport.width, viewport.height);
		}
	}
	
	float averageFramerate = Atlas::Clock::GetAverage();

	// ImGui rendering
	if (renderUI) {
		ImGui::NewFrame();

		auto& light = directionalLight;
		auto& volume = scene.irradianceVolume;
		auto& ssao = scene.ssao;
		auto& fog = scene.fog;

		bool openSceneNotFoundPopup = false;

		auto vecToString = [](auto vec) -> std::string {
			return std::to_string(vec.x) + ", "
				+ std::to_string(vec.y) + ", "
				+ std::to_string(vec.z);
		};

		if (ImGui::Begin("Settings", (bool*)0, 0)) {
			ImGui::Text(("Average frametime: " + std::to_string(averageFramerate * 1000.0f) + " ms").c_str());
			ImGui::Text(("Current frametime: " + std::to_string(deltaTime * 1000.0f) + " ms").c_str());
			ImGui::Text(("Camera location: " + vecToString(camera.location)).c_str());

			const char* items[] = { "Cornell box", "Sponza", "Bistro", "San Miguel", "Medieval"};
			int item_current = static_cast<int>(sceneSelection);
			ImGui::Combo("Select scene", &item_current, items, IM_ARRAYSIZE(items));

			if (item_current != sceneSelection) {
				auto newSceneSelection = static_cast<SceneSelection>(item_current);
				if (IsSceneAvailable(newSceneSelection)) {
					sceneSelection = newSceneSelection;
					UnloadScene();
					LoadScene();
				}
				else {
					openSceneNotFoundPopup = true;
				}
			}

			ImGui::Checkbox("Pathtrace", &pathTrace);

			if (pathTrace) ImGui::SliderInt("Pathtrace bounces", &rayTracingRenderer.bounces, 0, 100);

			if (ImGui::CollapsingHeader("General")) {
				static bool fullscreenMode = false;
				static bool vsyncMode = false;

				bool fullscreen = fullscreenMode;
				bool vsync = vsyncMode;

				ImGui::Checkbox("VSync", &vsync);
				ImGui::Checkbox("Fullscreen", &fullscreen);

				if (vsync != vsyncMode) {
					if (vsync) LockFramerate();
					else UnlockFramerate();
					vsyncMode = vsync;
				}
				if (fullscreen != fullscreenMode) {
					if (fullscreen) {
						windowWidth = window.GetWidth();
						windowHeight = window.GetHeight();
						window.SetSize(GetScreenSize().x, GetScreenSize().y);
						window.SetFullscreen(true);
					}
					else {
						window.SetSize(windowWidth, windowHeight);
						window.SetFullscreen(false);
					}
					fullscreenMode = fullscreen;
				}
			}

			if (ImGui::CollapsingHeader("Irradiance volume")) {
				ImGui::Text(("Probe count: " + vecToString(volume->probeCount)).c_str());
				ImGui::Checkbox("Enable volume", &volume->enable);
				ImGui::Checkbox("Update volume", &volume->update);
				ImGui::Checkbox("Visualize probes", &spheresVisible);
				ImGui::Checkbox("Sample emissives", &volume->sampleEmissives);
				ImGui::SliderFloat("Strength##DDGI", &volume->strength, 0.0f, 5.0f);
				ImGui::Separator();
				ImGui::Text("AABB");
				ImGui::SliderFloat3("Min", (float*)&volume->aabb.min, -200.0f, 200.0f);
				ImGui::SliderFloat3("Max", (float*)&volume->aabb.max, -200.0f, 200.0f);
				volume->SetAABB(volume->aabb);
				ImGui::Separator();
				ImGui::SliderFloat("Hysteresis", &volume->hysteresis, 0.0f, 1.0f, "%.3f", 0.5f);
				ImGui::SliderFloat("Sharpness", &volume->sharpness, 0.01f, 200.0f, "%.3f", 2.0f);
				ImGui::SliderFloat("Bias", &volume->bias, 0.0f, 1.0f);
				auto prevGamma = volume->gamma;
				ImGui::SliderFloat("Gamma exponent", &volume->gamma, 0.0f, 10.0f, "%.3f", 2.0f);
				if (prevGamma != volume->gamma) volume->ClearProbes();
			}
			if (ImGui::CollapsingHeader("Light")) {
				ImGui::Checkbox("Animate", &animateLight);
				ImGui::SliderFloat3("Direction", (float*)&light.direction, -1.0f, 1.0f);
				ImGui::ColorEdit3("Color", (float*)&light.color);
				ImGui::SliderFloat("Intensity##Light", &light.intensity, 0.0, 1000.0f, "%.3f", 2.0f);
				ImGui::Separator();
				ImGui::Text("Volumetric");
				ImGui::SliderFloat("Intensity##Volumetric", &light.GetVolumetric()->intensity, 0.0f, 1.0f);
				ImGui::Text("Shadow");
				ImGui::SliderFloat("Bias##Shadow", &light.GetShadow()->bias, 0.0f, 2.0f);
			}
			if (ImGui::CollapsingHeader("Ambient Occlusion")) {
				ImGui::Checkbox("Debug", &showAo);
				ImGui::Checkbox("Enable ambient occlusion", &ssao->enable);
				ImGui::SliderFloat("Radius", &ssao->radius, 0.0f, 10.0f);
				ImGui::SliderFloat("Strength", &ssao->strength, 0.0f, 20.0f, "%.3f", 2.0f);
			}
			if (ImGui::CollapsingHeader("Camera")) {
				ImGui::SliderFloat("Exposure", &camera.exposure, 0.0f, 10.0f);
			}
			if (ImGui::CollapsingHeader("Fog")) {
				ImGui::Checkbox("Enable##Fog", &fog->enable);
				fog->color = glm::pow(fog->color, 1.0f / vec3(2.2f));
				ImGui::ColorEdit3("Color##Fog", &fog->color[0]);
				fog->color = glm::pow(fog->color, vec3(2.2f));

				ImGui::SliderFloat("Density##Fog", &fog->density, 0.0f, 0.5f, "%.4f", 4.0f);
				ImGui::SliderFloat("Height##Fog", &fog->height, 0.0f, 300.0f, "%.3f", 4.0f);
				ImGui::SliderFloat("Height falloff##Fog", &fog->heightFalloff, 0.0f, 0.5f, "%.4f", 4.0f);
				ImGui::SliderFloat("Scattering anisotropy##Fog", &fog->scatteringAnisotropy, -1.0f, 1.0f, "%.3f", 2.0f);
			}
			if (ImGui::CollapsingHeader("Postprocessing")) {
				ImGui::Text("Temporal anti-aliasing");
				ImGui::Checkbox("Enable##TAA", &scene.postProcessing.taa.enable);
				ImGui::Checkbox("Enable slow mode##SlowMode", &slowMode);
				ImGui::SliderFloat("Jitter range##TAA", &scene.postProcessing.taa.jitterRange, 0.001f, 0.999f);
				ImGui::Separator();
				ImGui::Text("Sharpen filter");
				ImGui::Checkbox("Enable##Sharpen", &scene.postProcessing.sharpen.enable);
				ImGui::SliderFloat("Sharpness", &scene.postProcessing.sharpen.factor, 0.0f, 1.0f);
			}
			if (ImGui::CollapsingHeader("Controls")) {
				ImGui::Text("Use WASD for movement");
				ImGui::Text("Use left mouse click + mouse movement to look around");
				ImGui::Text("Use F11 to hide/unhide the UI");
			}
		}

		if (openSceneNotFoundPopup) {
			ImGui::OpenPopup("Scene not found");
		}

		if (ImGui::BeginPopupModal("Scene not found")) {
			ImGui::Text("Please download additional scenes with the download script in the data directory");
			ImGui::Text("There is a script for both Linux and Windows");
			ImGui::Text("Note: Not all scene might be downloadable");
			if (ImGui::Button("Close##SceneNotFound")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::Render();
		imguiWrapper.Render();
	}

	if (slowMode) { using namespace std::chrono_literals; std::this_thread::sleep_for(60ms); }


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

bool App::IsSceneAvailable(SceneSelection selection) {
	switch (selection) {
	case CORNELL: return Atlas::Loader::AssetLoader::FileExists("cornell/CornellBox-Original.obj");
	case SPONZA: return Atlas::Loader::AssetLoader::FileExists("sponza/sponza.obj");
	case BISTRO: return Atlas::Loader::AssetLoader::FileExists("bistro/mesh/exterior.obj");
	case SANMIGUEL: return Atlas::Loader::AssetLoader::FileExists("sanmiguel/san-miguel.obj");
	case MEDIEVAL: return Atlas::Loader::AssetLoader::FileExists("medieval/scene.fbx");
	}
}

bool App::LoadScene() {

	bool successful = false;

	DisplayLoadingScreen();

	Atlas::Texture::Cubemap sky;
	//else sky = Atlas::Texture::Cubemap("moonlit_golf_4k.hdr", 1024);

	if (sceneSelection == CORNELL) {
		if (!Atlas::Loader::AssetLoader::FileExists("cornell/CornellBox-Original.obj")) return false;

		mesh = Atlas::Mesh::Mesh("cornell/CornellBox-Original.obj");
		mesh.invertUVs = true;
		mesh.SetTransform(scale(mat4(1.0f), vec3(10.0f)));
		scene.irradianceVolume = new Atlas::Lighting::IrradianceVolume(mesh.data.aabb.Scale(1.10f), ivec3(20));
		scene.irradianceVolume->sampleEmissives = true;
		mesh.cullBackFaces = false;

		// Other scene related settings apart from the mesh
		directionalLight.intensity = 0.0f;
		directionalLight.GetVolumetric()->intensity = 0.0f;
		scene.irradianceVolume->SetRayCount(512, 32);

		// Setup camera
		camera.location = vec3(0.0f, 14.0f, 40.0f);
		camera.rotation = vec2(-3.14f, -0.1f);

		scene.fog->enable = false;
	}
	else if (sceneSelection == SPONZA) {
		if (!Atlas::Loader::AssetLoader::FileExists("sponza/sponza.obj")) return false;

		mesh = Atlas::Mesh::Mesh("sponza/sponza.obj");
		mesh.invertUVs = true;
		mesh.SetTransform(scale(mat4(1.0f), vec3(.05f)));
		scene.irradianceVolume = new Atlas::Lighting::IrradianceVolume(mesh.data.aabb.Scale(0.90f), ivec3(20));
		mesh.cullBackFaces = true;

		sky = Atlas::Texture::Cubemap("environment.hdr", 1024);

		// Other scene related settings apart from the mesh
		directionalLight.intensity = 100.0f;
		directionalLight.GetVolumetric()->intensity = 0.28f;
		scene.irradianceVolume->SetRayCount(128, 32);

		// Setup camera
		camera.location = vec3(30.0f, 25.0f, 0.0f);
		camera.rotation = vec2(-3.14f / 2.0f, 0.0f);

		scene.fog->enable = true;
	}
	else if (sceneSelection == BISTRO) {
		if (!Atlas::Loader::AssetLoader::FileExists("bistro/mesh/exterior.obj")) return false;

		mesh = Atlas::Mesh::Mesh("bistro/mesh/exterior.obj", false, 2048);
		mesh.invertUVs = true;
		mesh.SetTransform(scale(mat4(1.0f), vec3(.01f)));
		scene.irradianceVolume = new Atlas::Lighting::IrradianceVolume(mesh.data.aabb.Scale(0.90f), ivec3(20));
		mesh.cullBackFaces = false;

		sky = Atlas::Texture::Cubemap("environment.hdr", 1024);

		// Other scene related settings apart from the mesh
		directionalLight.intensity = 100.0f;
		directionalLight.GetVolumetric()->intensity = 0.28f;
		scene.irradianceVolume->SetRayCount(32, 32);

		// Setup camera
		camera.location = vec3(30.0f, 25.0f, 0.0f);
		camera.rotation = vec2(-3.14f / 2.0f, 0.0f);

		scene.fog->enable = true;
	}
	else if (sceneSelection == SANMIGUEL) {
		if (!Atlas::Loader::AssetLoader::FileExists("sanmiguel/san-miguel.obj")) return false;

		mesh = Atlas::Mesh::Mesh("sanmiguel/san-miguel.obj", false, 2048);
		mesh.invertUVs = true;
		mesh.SetTransform(scale(mat4(1.0f), vec3(2.0f)));
		scene.irradianceVolume = new Atlas::Lighting::IrradianceVolume(mesh.data.aabb.Scale(1.0f), ivec3(20));
		mesh.cullBackFaces = false;

		sky = Atlas::Texture::Cubemap("environment.hdr", 1024);

		// Other scene related settings apart from the mesh
		directionalLight.intensity = 10.0f;
		directionalLight.GetVolumetric()->intensity = 0.28f;
		scene.irradianceVolume->SetRayCount(32, 32);

		// Setup camera
		camera.location = vec3(30.0f, 25.0f, 0.0f);
		camera.rotation = vec2(-3.14f / 2.0f, 0.0f);

		scene.fog->enable = true;
	}
	else if (sceneSelection == MEDIEVAL) {
		if (!Atlas::Loader::AssetLoader::FileExists("medieval/scene.fbx")) return false;

		mesh = Atlas::Mesh::Mesh("medieval/scene.fbx", false, 2048);
		mesh.invertUVs = true;
		mesh.SetTransform(scale(glm::rotate(glm::mat4(1.0f), -3.14f / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)), vec3(2.0f)));
		// Metalness is set to 0.9f
		for (auto& material : mesh.data.materials) material.metalness = 0.0f;

		scene.irradianceVolume = new Atlas::Lighting::IrradianceVolume(mesh.data.aabb.Scale(1.0f), ivec3(20));
		//mesh.cullBackFaces = false;

		sky = Atlas::Texture::Cubemap("environment.hdr", 1024);

		// Other scene related settings apart from the mesh
		directionalLight.intensity = 10.0f;
		directionalLight.GetVolumetric()->intensity = 0.08f;
		scene.irradianceVolume->SetRayCount(128, 32);

		// Setup camera
		camera.location = vec3(30.0f, 25.0f, 0.0f);
		camera.rotation = vec2(-3.14f / 2.0f, 0.0f);

		scene.fog->enable = true;
	}

	meshActor = Atlas::Actor::StaticMeshActor(&mesh, mat4(1.0f));

	scene.Add(&meshActor);

	scene.sky.probe = new Atlas::Lighting::EnvironmentProbe(sky);

	camera.Update();
	scene.Update(&camera, 1.0f);
	scene.BuildRTStructures();

	// Reset input handlers
	keyboardHandler.Reset(&camera);
	mouseHandler.Reset(&camera);

	return successful;

}

void App::UnloadScene() {

	scene.Remove(&meshActor);

	delete scene.sky.probe;
	delete scene.irradianceVolume;

}

Atlas::EngineInstance* GetEngineInstance() {

	return new App();

}
