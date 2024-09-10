#include "App.h"

#include <scene/SceneSerializer.h>

#include <chrono>
#include <thread>

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
#ifdef AE_BUNDLE
    .assetDirectory = "../Resources/data",
#else
    .assetDirectory = "../../data",
#endif
    .shaderDirectory = "shader"
};

using namespace Atlas::Scene::Components;
using namespace Atlas::Scene::Prefabs;
using namespace Atlas::ImguiExtension;

void App::LoadContent() {

    renderTarget = Atlas::CreateRef<Atlas::Renderer::RenderTarget>(1920, 1080);

    viewport = Atlas::CreateRef<Atlas::Viewport>(0, 0, renderTarget->GetWidth(), renderTarget->GetHeight());

    auto icon = Atlas::Texture::Texture2D("icon.png");
    window.SetIcon(&icon);

    loadingTexture = Atlas::CreateRef<Atlas::Texture::Texture2D>("loading.png");

    font = Atlas::CreateRef<Atlas::Font>("font/roboto.ttf", 22.0f, 5);

    scene = Atlas::CreateRef<Atlas::Scene::Scene>("demoScene", glm::vec3(-2048.0f), glm::vec3(2048.0f));

    cameraEntity = scene->CreateEntity();
    auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 400.0f,
        glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));

    mouseHandler = Atlas::Input::MouseHandler(1.5f, 8.0f);
    keyboardHandler = Atlas::Input::KeyboardHandler(7.0f, 5.0f);
    controllerHandler = Atlas::Input::ControllerHandler(1.0f, 5.0f, 10.0f, 5000.0f);

    Atlas::Events::EventManager::KeyboardEventDelegate.Subscribe(
        [this](Atlas::Events::KeyboardEvent event) {
            if (event.keyCode == Keycode::KeyEscape) {
                Exit();
            }
            if (event.keyCode == Keycode::KeyF11 && event.state == AE_BUTTON_RELEASED) {
                renderUI = !renderUI;
            }
            if (event.keyCode == Keycode::KeyLeftShift && event.state == AE_BUTTON_PRESSED) {
                keyboardHandler.speed = cameraSpeed * 4.0f;
            }
            if (event.keyCode == Keycode::KeyLeftShift && event.state == AE_BUTTON_RELEASED) {
                keyboardHandler.speed = cameraSpeed;
            }
        });

    Atlas::Events::EventManager::MouseButtonEventDelegate.Subscribe(
        [this](Atlas::Events::MouseButtonEvent event) {
            if (event.button == AE_MOUSEBUTTON_RIGHT) {
                shootSphere = event.down && shootSpheresEnabled;
            }
        });

    Atlas::PipelineManager::EnableHotReload();

    directionalLightEntity = scene->CreateEntity();
    auto& directionalLight = directionalLightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);

    directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 1.0f);
    directionalLight.color = glm::vec3(255, 236, 209) / 255.0f;
    directionalLight.AddDirectionalShadow(200.0f, 3.0f, 4096, 0.125f,
        glm::vec3(0.0f), glm::vec4(-100.0f, 100.0f, -70.0f, 120.0f));
    directionalLight.isMain = true;

    scene->ao = Atlas::CreateRef<Atlas::Lighting::AO>(16);
    scene->ao->rt = true;
    // Use SSGI by default
    scene->ao->enable = false;
    scene->reflection = Atlas::CreateRef<Atlas::Lighting::Reflection>();
    scene->reflection->useShadowMap = true;

    scene->fog = Atlas::CreateRef<Atlas::Lighting::Fog>();
    scene->fog->enable = true;
    scene->fog->density = 0.0002f;
    scene->fog->heightFalloff = 0.0284f;
    scene->fog->height = 0.0f;

    scene->sky.atmosphere = Atlas::CreateRef<Atlas::Lighting::Atmosphere>();

    scene->postProcessing.taa = Atlas::PostProcessing::TAA(0.99f);
    scene->postProcessing.sharpen.enable = true;
    scene->postProcessing.sharpen.factor = 0.15f;

    scene->sss = Atlas::CreateRef<Atlas::Lighting::SSS>();

    scene->ssgi = Atlas::CreateRef<Atlas::Lighting::SSGI>();
    scene->rtgi = Atlas::CreateRef<Atlas::Lighting::RTGI>();
    scene->rtgi->enable = graphicsDevice->support.hardwareRayTracing;
    scene->rtgi->useShadowMap = true;

    scene->physicsWorld = Atlas::CreateRef<Atlas::Physics::PhysicsWorld>();
    scene->rayTracingWorld = Atlas::CreateRef<Atlas::RayTracing::RayTracingWorld>();

    LoadScene();

    imguiWrapper = Atlas::CreateRef<Atlas::ImguiExtension::ImguiWrapper>();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    imguiWrapper->Load(&window);

}

void App::UnloadContent() {

    UnloadScene();
    imguiWrapper->Unload();

}

void App::Update(float deltaTime) {
    
    scene->WaitForAsyncWorkCompletion();

    if (sceneReload) {
        UnloadScene();
        LoadScene();
        scene->WaitForAsyncWorkCompletion();
        sceneReload = false;
    }

    const ImGuiIO& io = ImGui::GetIO();

    imguiWrapper->Update(&window, deltaTime);

    if (io.WantCaptureMouse) {
        mouseHandler.lock = true;
    }
    else {
        mouseHandler.lock = false;
    }

    auto& camera = cameraEntity.GetComponent<CameraComponent>();

    if (controllerHandler.IsControllerAvailable()) {
        controllerHandler.Update(camera, deltaTime);
    }
    else {
        mouseHandler.Update(camera, deltaTime);
        keyboardHandler.Update(camera, deltaTime);
    }

    if (rotateCamera) {
        camera.rotation.y += rotateCameraSpeed * cos(Atlas::Clock::Get());
    }

    if (moveCamera) {
        camera.location += camera.right * moveCameraSpeed * cos(Atlas::Clock::Get());
    }

    if (sceneSelection == SPONZA) {
        auto meshEntitySubset = scene->GetSubset<MeshComponent, TransformComponent>();

        int idx = 0;
        for (auto entity : meshEntitySubset) {

            if (idx++ >= 3)
                continue;

            const auto& [meshComponent, transformComponent] = meshEntitySubset.Get(entity);

            if (!meshComponent.mesh.IsLoaded())
                continue;

            if (meshComponent.mesh->name == "chromesphere") {
                float height = (sinf(Atlas::Clock::Get() / 5.0f) + 1.0f) * 20.0f;
                auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, height, .0f));

                transformComponent.Set(matrix);
            }
            else if (meshComponent.mesh->name == "metallicwall") {
                auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 0.0f, -2.0f));
                matrix = glm::rotate(matrix, Atlas::Clock::Get() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));

                transformComponent.Set(matrix);
            }

        }
    }

    if (scene->IsFullyLoaded() && emitSpheresEnabled) {

        static float lastSpawn = 0.0f;

        if (Atlas::Clock::Get() - emitSpawnRate > lastSpawn) {
            auto x = (2.0f * Atlas::Common::Random::SampleFastUniformFloat() - 1.0f) * 20.0f;
            auto z = (2.0f * Atlas::Common::Random::SampleFastUniformFloat() - 1.0f) * 20.0f;

            auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, 100.0f, z));

            auto entity = scene->CreatePrefab<MeshInstance>(meshes.back(), matrix, false);

            auto shapeSettings = Atlas::Physics::SphereShapeSettings{
                .radius = meshes.back()->data.radius,
                .density = sphereDensity,
                .scale = glm::vec3(sphereScale),
            };
            auto shape = Atlas::Physics::ShapesManager::CreateShape(shapeSettings);

            auto bodySettings = Atlas::Physics::BodyCreationSettings{
                .objectLayer = Atlas::Physics::Layers::Movable,
                .restitution = sphereRestitution,
                .shape = shape,
            };
            auto& rigidBodyComponent = entity.AddComponent<RigidBodyComponent>(bodySettings);
            rigidBodyComponent.SetRestitution(sphereRestitution);

            entities.push_back(entity);
            lastSpawn = Atlas::Clock::Get();
        }

    }

    if (scene->IsFullyLoaded() && shootSphere) {

        static float lastSpawn = 0.0f;


        if (Atlas::Clock::Get() - shootSpawnRate > lastSpawn) {
            auto shapeSettings = Atlas::Physics::SphereShapeSettings{
                .radius = meshes.back()->data.radius,
                .density = sphereDensity,
                .scale = glm::vec3(sphereScale),
            };
            auto shape = Atlas::Physics::ShapesManager::CreateShape(shapeSettings);

            auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(camera.GetLocation() +
                camera.direction * meshes.back()->data.radius * 2.0f));
            auto entity = scene->CreatePrefab<MeshInstance>(meshes.back(), matrix, false);

            auto bodySettings = Atlas::Physics::BodyCreationSettings{
                .objectLayer = Atlas::Physics::Layers::Movable,
                .motionQuality = Atlas::Physics::MotionQuality::LinearCast,
                .linearVelocity = camera.direction * shootVelocity,
                .restitution = sphereRestitution,
                .shape = shape,
            };
            entity.AddComponent<RigidBodyComponent>(bodySettings);

            entities.push_back(entity);
            lastSpawn = Atlas::Clock::Get();
        }

    }

    scene->Timestep(deltaTime);
    scene->Update();

    CheckLoadScene();

}

void App::Render(float deltaTime) {

    static bool firstFrame = true;
    static bool animateLight = false;
    static bool pathTrace = true;
    static bool debugAo = false;
    static bool debugReflection = false;
    static bool debugClouds = false;
    static bool debugSSS = false;
    static bool debugSSGI = false;
    static bool debugRTGI = false;
    static bool debugMotion = false;
    static bool slowMode = false;

    static float cloudDepthDebug = 0.0f;

    graphicsDevice->WaitForPreviousFrameSubmission();

#ifndef AE_HEADLESS
    auto windowFlags = window.GetFlags();
    if (windowFlags & AE_WINDOW_HIDDEN || windowFlags & AE_WINDOW_MINIMIZED || !(windowFlags & AE_WINDOW_SHOWN)) {
        // If we take the early way out we need to make sure that stuff is completed (usually main renderer takes care of that)
        scene->WaitForAsyncWorkCompletion();
        return;
    }
#endif

    if (!loadingComplete) {
        DisplayLoadingScreen(deltaTime);
        // If we take the early way out we need to make sure that stuff is completed
        scene->WaitForAsyncWorkCompletion();
        return;
    }

    auto& directionalLight = directionalLightEntity.GetComponent<LightComponent>();
    if (animateLight) directionalLight.properties.directional.direction
        = glm::vec3(0.0f, -1.0f, sin(Atlas::Clock::Get() / 10.0f));

    viewport->Set(0, 0, renderTarget->GetWidth(), renderTarget->GetHeight());

    if (pathTrace) {
        mainRenderer->PathTraceScene(viewport, renderTarget, scene);
    }
    else {
        mainRenderer->RenderScene(viewport, renderTarget, scene);

        auto debug = debugAo || debugReflection || debugClouds || debugSSS || debugSSGI || debugRTGI || debugMotion;

        if (debug && graphicsDevice->swapChain->isComplete) {
            auto commandList = graphicsDevice->GetCommandList(Atlas::Graphics::GraphicsQueue);
            commandList->BeginCommands();
            commandList->BeginRenderPass(graphicsDevice->swapChain, true);

            if (debugAo) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->aoTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugReflection) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->reflectionTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugClouds) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->volumetricCloudsTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugSSS) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->sssTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugSSGI || debugRTGI) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->giTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugMotion) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport,
                    renderTarget->GetData(Atlas::Renderer::FULL_RES)->velocityTexture.get(),
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 10.0f, false, true);
            }

            commandList->EndRenderPass();
            commandList->EndCommands();
            graphicsDevice->SubmitCommandList(commandList);
        }
    }

    float averageFramerate = Atlas::Clock::GetAverage();

    // ImGui rendering
    if (renderUI) {
        static bool recreateSwapchain = false;

        ImGui::NewFrame();

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        auto& light = directionalLightEntity.GetComponent<LightComponent>();
        auto& volume = scene->irradianceVolume;
        auto& ao = scene->ao;
        auto& fog = scene->fog;
        auto& reflection = scene->reflection;
        auto& clouds = scene->sky.clouds;
        auto& sss = scene->sss;
        auto& ssgi = scene->ssgi;
        auto& rtgi = scene->rtgi;
        auto& postProcessing = scene->postProcessing;

        bool openSceneNotFoundPopup = false;

        auto floatToString = [](auto number) -> std::string {
            auto str = std::to_string(number);
            auto pos = str.find(".");
            if (pos != std::string::npos)
                return str.substr(0, pos + 4);
            return str;
            };

        auto vecToString = [=](auto vec) -> std::string {
            return floatToString(vec.x) + ", "
                + floatToString(vec.y) + ", "
                + floatToString(vec.z);
            };

        uint32_t triangleCount = 0;
        auto sceneAABB = Atlas::Volume::AABB();
        for (auto& mesh : meshes) {
            if (!mesh.IsLoaded())
                continue;
            sceneAABB.Grow(mesh->data.aabb);
            triangleCount += mesh->data.GetIndexCount() / 3;
        }

        if (ImGui::Begin("Settings", (bool*)0, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (pathTrace) ImGui::Text("Samples: %d", mainRenderer->pathTracingRenderer.GetSampleCount());
            ImGui::Text("Average frametime: %.3f ms", averageFramerate * 1000.0f);
            ImGui::Text("Current frametime: %.3f ms", deltaTime * 1000.0f);
            ImGui::Text("Camera location: %s", vecToString(camera.location).c_str());
            ImGui::Text("Scene dimensions: %s to %s", vecToString(sceneAABB.min).c_str(), vecToString(sceneAABB.max).c_str());
            ImGui::Text("Scene triangle count: %d", triangleCount);
            ImGui::Text("Number of entities: %zu", scene->GetEntityCount());

            {
                const char* items[] = { "Cornell box", "Sponza", "San Miguel",
                                        "New Sponza", "Bistro", "Medieval", "Pica Pica",
                                        "Subway", "Materials", "Forest", "Emerald square",
                                        "Flying world" };
                int currentItem = static_cast<int>(sceneSelection);
                ImGui::Combo("Select scene", &currentItem, items, IM_ARRAYSIZE(items));

                if (currentItem != sceneSelection) {
                    auto newSceneSelection = static_cast<SceneSelection>(currentItem);
                    if (IsSceneAvailable(newSceneSelection)) {
                        sceneSelection = newSceneSelection;
                        sceneReload = true;
                    }
                    else {
                        openSceneNotFoundPopup = true;
                    }
                }
            }

            if (ImGui::CollapsingHeader("General")) {
                static bool fullscreenMode = false;
                static bool vsyncMode = false;
                bool hdrMode = graphicsDevice->swapChain->IsHDR();

                bool fullscreen = fullscreenMode;
                bool vsync = vsyncMode;
                bool hdr = hdrMode;

                ImGui::Checkbox("VSync##General", &vsync);
                ImGui::Checkbox("Fullscreen##General", &fullscreen);
                ImGui::Checkbox("HDR##General", &hdr);

                if (vsync != vsyncMode || hdr != hdrMode) {
                    Atlas::Graphics::ColorSpace colorSpace = Atlas::Graphics::SRGB_NONLINEAR;
                    if (hdr) {
                        colorSpace = Atlas::Graphics::HDR10_HLG;
                    }
                    graphicsDevice->SubmitFrame();
                    if (vsync) graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_FIFO_KHR, colorSpace);
                    else graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_IMMEDIATE_KHR, colorSpace);
                    vsyncMode = vsync;
                    hdrMode = hdr;
                    imguiWrapper->RecreateImGuiResources();
                    graphicsDevice->WaitForIdle();
                    recreateSwapchain = true;
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

                const char* items[] = { "1280x720", "1920x1080", "2560x1440", "3840x2160" };
                static int resolution = 1;
                int currentItem = resolution;
                ImGui::Combo("Resolution##Rendering", &currentItem, items, IM_ARRAYSIZE(items));

                if (currentItem != resolution) {
                    resolution = currentItem;
                    switch (resolution) {
                    case 0: SetResolution(1280, 720); break;
                    case 1: SetResolution(1920, 1080); break;
                    case 2: SetResolution(2560, 1440); break;
                    case 3: SetResolution(3840, 2160); break;
                    }
                }

                auto resolutionScale = renderTarget->GetScalingFactor();
                ImGui::DragFloat("Resolution scale##Rendering", &resolutionScale, 0.01f, 0.1f, 1.0f);

                if (renderTarget->GetScalingFactor() != resolutionScale)
                    renderTarget->SetScalingFactor(resolutionScale);

            }
            if (ImGui::CollapsingHeader("Pathtracing")) {
                bool pathTraceEnabled = pathTrace;
                ImGui::Checkbox("Enable##Pathtrace", &pathTrace);
                ImGui::SliderInt("Bounces##Pathtrace", &mainRenderer->pathTracingRenderer.bounces, 0, 100);
                ImGui::Checkbox("Sample emissives##Pathtrace", &mainRenderer->pathTracingRenderer.sampleEmissives);
                ImGui::Text("Realtime");
                ImGui::Checkbox("Realtime##Pathtrace", &mainRenderer->pathTracingRenderer.realTime);
                ImGui::SliderInt("Samples per frame##Pathtrace", &mainRenderer->pathTracingRenderer.realTimeSamplesPerFrame, 1, 100);
                ImGui::Text("Realtime denoiser");
                ImGui::SliderInt("Max accumulated frames##Pathtrace", &mainRenderer->pathTracingRenderer.historyLengthMax, 1, 256);
                ImGui::SliderFloat("Current clip##Pathtrace", &mainRenderer->pathTracingRenderer.currentClipFactor, 0.1f, 4.0f);
                ImGui::SliderFloat("Max history clip##Pathtrace", &mainRenderer->pathTracingRenderer.historyClipMax, 0.0f, 1.0f);

                scene->rayTracingWorld->includeObjectHistory = pathTrace;
            }
            if (ImGui::CollapsingHeader("DDGI")) {
                irradianceVolumePanel.Render(volume, scene);
            }
            if (ImGui::CollapsingHeader("RTGI")) {
                ImGui::Checkbox("Debug##RTGI", &debugRTGI);
                rtgiPanel.Render(rtgi, renderTarget);
            }
            if (ImGui::CollapsingHeader("SSGI")) {
                ImGui::Checkbox("Debug##SSGI", &debugSSGI);
                ssgiPanel.Render(ssgi, renderTarget);
            }
            if (ImGui::CollapsingHeader("Light")) {
                ImGui::Checkbox("Animate", &animateLight);
                ImGui::SliderFloat3("Direction", &light.properties.directional.direction[0], -1.0f, 1.0f);
                ImGui::ColorEdit3("Color", &light.color[0]);
                ImGui::SliderFloat("Intensity##Light", &light.intensity, 0.0, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::Separator();
                ImGui::Text("Shadow");
                auto shadow = light.shadow;
                const char* gridResItems[] = { "512x512", "1024x1024", "2048x2048", "4096x4096", "8192x8192" };
                int currentItem = 0;
                if (shadow->resolution == 512) currentItem = 0;
                if (shadow->resolution == 1024) currentItem = 1;
                if (shadow->resolution == 2048) currentItem = 2;
                if (shadow->resolution == 4096) currentItem = 3;
                if (shadow->resolution == 8192) currentItem = 4;
                auto prevItem = currentItem;
                ImGui::Combo("Resolution##Shadow", &currentItem, gridResItems, IM_ARRAYSIZE(gridResItems));

                if (currentItem != prevItem) {
                    switch (currentItem) {
                    case 0: shadow->SetResolution(512); break;
                    case 1: shadow->SetResolution(1024); break;
                    case 2: shadow->SetResolution(2048); break;
                    case 3: shadow->SetResolution(4096); break;
                    case 4: shadow->SetResolution(8192); break;
                    }
                }
                ImGui::SliderFloat("Bias##Shadow", &shadow->bias, 0.0f, 2.0f);
                ImGui::DragFloat("Edge softness##Shadow", &shadow->edgeSoftness, 0.005f, 0.0f, 1.0f);
            }
            if (ImGui::CollapsingHeader("Screen-space shadows")) {
                ImGui::Checkbox("Debug##SSS", &debugSSS);
                sssPanel.Render(sss);
            }
            if (ImGui::CollapsingHeader("Ambient Occlusion")) {
                ImGui::Checkbox("Debug##Ao", &debugAo);
                ImGui::Checkbox("Enable ambient occlusion##Ao", &ao->enable);
                ImGui::Checkbox("Enable raytracing (preview)##Ao", &ao->rt);
                ImGui::Checkbox("Opacity check##Ao", &ao->opacityCheck);
                ImGui::SliderFloat("Radius##Ao", &ao->radius, 0.0f, 10.0f);
                ImGui::SliderFloat("Strength##Ao", &ao->strength, 0.0f, 20.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                //ImGui::SliderInt("Sample count##Ao", &ao->s, 0.0f, 20.0f, "%.3f", 2.0f);
            }
            if (ImGui::CollapsingHeader("Reflection")) {
                ImGui::Checkbox("Debug##Reflection", &debugReflection);
                reflectionPanel.Render(reflection, renderTarget);
            }
            if (ImGui::CollapsingHeader("Camera")) {
                ImGui::SliderFloat("Exposure##Camera", &camera.exposure, 0.0f, 10.0f);
                ImGui::SliderFloat("Speed##Camera", &cameraSpeed, 0.0f, 20.0f);
                ImGui::SliderFloat("FOV##Camera", &camera.fieldOfView, 0.0f, 90.0f);
                keyboardHandler.speed = cameraSpeed;
                ImGui::Separator();
                ImGui::Text("Camera debugging");
                ImGui::Checkbox("Show motion vectors##MotionVecs", &debugMotion);
                ImGui::Checkbox("Move camera", &moveCamera);
                ImGui::Checkbox("Rotate camera", &rotateCamera);
            }
            if (ImGui::CollapsingHeader("Fog")) {
                fogPanel.Render(fog, renderTarget);
            }
            if (ImGui::CollapsingHeader("Clouds")) {
                ImGui::Checkbox("Debug##Clouds", &debugClouds);
                volumetricCloudsPanel.Render(clouds, renderTarget);
            }
            if (ImGui::CollapsingHeader("Wind")) {
                windPanel.Render(imguiWrapper, scene->wind);
            }
            if (ImGui::CollapsingHeader("Postprocessing")) {
                postProcessingPanel.Render(postProcessing);
            }
            if (ImGui::CollapsingHeader("Physics")) {
                ImGui::Checkbox("Pause simulation##Phyiscs", &scene->physicsWorld->pauseSimulation);
                ImGui::Text("Sphere body");
                ImGui::SliderFloat("Sphere scale##PhysicsBody", &sphereScale, 1.0f, 10.0f);
                ImGui::SliderFloat("Sphere density##PhysicsBody", &sphereDensity, 1.0f, 100.0f);
                ImGui::SliderFloat("Sphere restitution##PhysicsBody", &sphereRestitution, 0.0f, 1.0f);
                ImGui::Text("Sphere emitter");
                ImGui::Checkbox("Enable##PhysicsEmitter", &emitSpheresEnabled);
                ImGui::SliderFloat("Spawn rate##PhysicsEmitter", &emitSpawnRate, 0.001f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Shoot spheres");
                ImGui::Checkbox("Enable##PhysicsShoot", &shootSpheresEnabled);
                ImGui::SliderFloat("Spawn rate##PhysicsShoot", &shootSpawnRate, 0.001f, 1.0f);
                ImGui::SliderFloat("Velocity##PhysicsShoot", &shootVelocity, 0.0f, 100.0f);

                if (ImGui::Button("Save state##Physics")) {
                    scene->physicsWorld->SaveState();
                }
                ImGui::SameLine();
                if (ImGui::Button("Restore state##Physics")) {
                    scene->physicsWorld->RestoreState();
                }
            }
            if (ImGui::CollapsingHeader("Materials")) {
                int32_t id = 0;
                auto materials = scene->GetMaterials();
                materialsPanel.Render(imguiWrapper, materials);
            }
            if (ImGui::CollapsingHeader("Controls")) {
                ImGui::Text("Use WASD for movement");
                ImGui::Text("Use left mouse click + mouse movement to look around");
                ImGui::Text("Use F11 to hide/unhide the UI");
            }
            if (ImGui::CollapsingHeader("Profiler")) {
                gpuProfilerPanel.Render();
            }
        }

        ImGui::End();

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

        if (!recreateSwapchain) {
            imguiWrapper->Render();
        }

        recreateSwapchain = false;

    }

    if (slowMode) { using namespace std::chrono_literals; std::this_thread::sleep_for(60ms); }

    if (firstFrame) {
        // We want to get rid of the current average
        // window which includes the loading times
        Atlas::Clock::ResetAverage();
        firstFrame = false;
    }
    
    scene->WaitForAsyncWorkCompletion();

}

void App::DisplayLoadingScreen(float deltaTime) {

    auto commandList = graphicsDevice->GetCommandList();

    commandList->BeginCommands();
    graphicsDevice->swapChain->colorClearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->BeginRenderPass(graphicsDevice->swapChain, true);

    auto windowSize = window.GetDrawableSize();

    float width = float(loadingTexture->width);
    float height = float(loadingTexture->height);

    float x = windowSize.x / 2 - width / 2;
    float y = windowSize.y / 2 - height / 2;

    static float rotation = 0.0f;

    rotation += deltaTime * abs(sin(Atlas::Clock::Get())) * 10.0f;

    mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport,
        loadingTexture.get(), x, y, width, height, rotation);

    float textWidth, textHeight;
    font->ComputeDimensions("Loading...", 2.0f, &textWidth, &textHeight);

    x = windowSize.x / 2 - textWidth / 2;
    y = windowSize.y / 2 - textHeight / 2 + float(loadingTexture->height) + 20.0f;

    viewport->Set(0, 0, windowSize.x, windowSize.y);
    mainRenderer->textRenderer.Render(commandList, viewport, font,
        "Loading...", x, y, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f);

    commandList->EndRenderPass();
    commandList->EndCommands();

    graphicsDevice->SubmitCommandList(commandList);

}

bool App::IsSceneAvailable(SceneSelection selection) {
    switch (selection) {
    case CORNELL: return Atlas::Loader::AssetLoader::FileExists("cornell/CornellBox-Original.obj");
    case SPONZA: return Atlas::Loader::AssetLoader::FileExists("sponza/sponza.obj");
    case BISTRO: return Atlas::Loader::AssetLoader::FileExists("bistro/mesh/exterior.obj");
    case SANMIGUEL: return Atlas::Loader::AssetLoader::FileExists("sanmiguel/san-miguel-low-poly.obj");
    case MEDIEVAL: return Atlas::Loader::AssetLoader::FileExists("medieval/scene.fbx");
    case PICAPICA: return Atlas::Loader::AssetLoader::FileExists("pica pica/mesh/scene.gltf");
    case SUBWAY: return Atlas::Loader::AssetLoader::FileExists("subway/scene.gltf");
    case MATERIALS: return Atlas::Loader::AssetLoader::FileExists("material demo/materials.obj");
    case FOREST: return Atlas::Loader::AssetLoader::FileExists("forest/forest.gltf");
    case EMERALDSQUARE: return Atlas::Loader::AssetLoader::FileExists("emeraldsquare/square.gltf");
    case FLYINGWORLD: return Atlas::Loader::AssetLoader::FileExists("flying world/scene.gltf");
    case NEWSPONZA: return Atlas::Loader::AssetLoader::FileExists("newsponza/main/NewSponza_Main_Blender_glTF.gltf") &&
        Atlas::Loader::AssetLoader::FileExists("newsponza/candles/NewSponza_100sOfCandles_glTF_OmniLights.gltf") &&
        Atlas::Loader::AssetLoader::FileExists("newsponza/curtains/NewSponza_Curtains_glTF.gltf") &&
        Atlas::Loader::AssetLoader::FileExists("newsponza/ivy/NewSponza_IvyGrowth_glTF.gltf");
    default: return false;
    }
}

bool App::LoadScene() {

    bool successful = false;
    loadingComplete = false;

    Atlas::ResourceHandle<Atlas::Texture::Cubemap> sky;
    auto& directionalLight = directionalLightEntity.GetComponent<LightComponent>();

    directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 1.0f);

    auto& camera = cameraEntity.GetComponent<CameraComponent>();

    scene->sky.clouds = Atlas::CreateRef<Atlas::Lighting::VolumetricClouds>();
    scene->sky.clouds->minHeight = 1400.0f;
    scene->sky.clouds->maxHeight = 1700.0f;
    scene->sky.clouds->castShadow = false;

    scene->sky.probe = nullptr;
    scene->sky.clouds->enable = true;
    scene->sss->enable = true;

    using namespace Atlas::Loader;

    std::vector<glm::mat4> transforms;
    if (sceneSelection == CORNELL) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "cornell/CornellBox-Original.obj", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::scale(glm::mat4(1.0f), glm::vec3(10.0f)));

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 0.0f;

        // Setup camera
        camera.location = glm::vec3(0.0f, 14.0f, 40.0f);
        camera.rotation = glm::vec2(-3.14f, -0.1f);

        scene->fog->enable = false;
        scene->fog->volumetricIntensity = 0.0f;
    }
    else if (sceneSelection == SPONZA) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "sponza/sponza.obj", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::scale(glm::mat4(1.0f), glm::vec3(.05f)));

        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "metallicwall.gltf", ModelImporter::ImportMesh, Atlas::Mesh::MeshMobility::Movable, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(1.0f);

        // Other scene related settings apart from the mesh
        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 0.33f);
        directionalLight.intensity = 100.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 0.125f;

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.28f;
    }
    else if (sceneSelection == BISTRO) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "bistro/mesh/exterior.obj", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::scale(glm::mat4(1.0f), glm::vec3(.015f)));

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 100.0f;

        // Setup camera
        camera.location = glm::vec3(-21.0f, 8.0f, 1.0f);
        camera.rotation = glm::vec2(3.14f / 2.0f, 0.0f);
        camera.exposure = 0.125f;

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.28f;
    }
    else if (sceneSelection == SANMIGUEL) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "sanmiguel/san-miguel-low-poly.obj", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)));

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 100.0f;
        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, -1.0f);

        // Setup camera
        camera.location = glm::vec3(45.0f, 26.0f, 17.0f);
        camera.rotation = glm::vec2(-4.14f / 2.0f, -.6f);
        camera.exposure = 2.5f;

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.28f;
    }
    else if (sceneSelection == MEDIEVAL) {
        meshes.reserve(1);

        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "medieval/scene.fbx", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(1.0f);

        // Metalness is set to 0.9f
        //for (auto& material : mesh.data.materials) material.metalness = 0.0f;

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 10.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == PICAPICA) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "pica pica/mesh/scene.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::rotate(glm::mat4(1.0f), -3.14f / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 10.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == SUBWAY) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "subway/scene.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(1.0f);

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 10.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == MATERIALS) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "material demo/materials.obj", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.push_back(glm::scale(glm::vec3(8.0f)));

        sky = Atlas::ResourceManager<Atlas::Texture::Cubemap>::GetOrLoadResourceAsync("environment.hdr");
        probe = Atlas::Lighting::EnvironmentProbe(sky);
        scene->sky.probe = Atlas::CreateRef(probe);

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 10.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
        scene->sky.clouds->enable = false;
        scene->sss->enable = true;
        scene->fog->volumetricIntensity = 0.0f;
    }
    else if (sceneSelection == FOREST) {
        auto otherScene = Atlas::Loader::ModelImporter::ImportScene("forest/forest.gltf", -glm::vec3(2048.0f), glm::vec3(2048.0f), 5);
        otherScene->Timestep(1.0f);
        otherScene->Update();

        CopyActors(otherScene);

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 50.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == EMERALDSQUARE) {
        auto otherScene = Atlas::Loader::ModelImporter::ImportScene("emeraldsquare/square.gltf", -glm::vec3(2048.0f), glm::vec3(2048.0f), 5);
        otherScene->Timestep(1.0f);
        otherScene->Update();

        CopyActors(otherScene);

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 10.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == FLYINGWORLD) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "flying world/scene.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(glm::scale(glm::vec3(0.01f)));

        // Metalness is set to 0.9f
        //for (auto& material : mesh.data.materials) material.metalness = 0.0f;

        // Other scene related settings apart from the mesh
        directionalLight.intensity = 50.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->sky.clouds->minHeight = 700.0f;
        scene->sky.clouds->maxHeight = 1000.0f;
        scene->sky.clouds->densityMultiplier = 0.65f;
        scene->sky.clouds->heightStretch = 1.0f;

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.08f;
    }
    else if (sceneSelection == NEWSPONZA) {
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/main/NewSponza_Main_Blender_glTF.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(glm::scale(glm::vec3(4.0f)));
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/candles/NewSponza_100sOfCandles_glTF_OmniLights.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(glm::scale(glm::vec3(4.0f)));
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/curtains/NewSponza_Curtains_glTF.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(glm::scale(glm::vec3(4.0f)));
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/ivy/NewSponza_IvyGrowth_glTF.gltf", ModelImporter::ImportMesh, false, 2048
        );
        meshes.push_back(mesh);
        transforms.emplace_back(glm::scale(glm::vec3(4.0f)));

        // Other scene related settings apart from the mesh
        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 0.33f);
        directionalLight.intensity = 100.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

        scene->fog->enable = true;
        scene->fog->volumetricIntensity = 0.28f;
    }

    // scene.sky.probe = std::make_shared<Atlas::Lighting::EnvironmentProbe>(sky);

    // Load chrome sphere for every scene in order to test physics
    auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
        "chromesphere.gltf", ModelImporter::ImportMesh, Atlas::Mesh::MeshMobility::Movable, false, 2048
    );
    meshes.push_back(mesh);
    transforms.emplace_back(1.0f);

    if (sceneSelection != FOREST && sceneSelection != EMERALDSQUARE) {
        auto meshCount = 0;
        for (auto& mesh : meshes) {
            // Only Sponza scene gets extra moving ball
            if (mesh.GetID() == meshes.back().GetID() && sceneSelection != SPONZA)
                continue;

            auto isStatic = meshCount == 0;
            auto entity = scene->CreatePrefab<MeshInstance>(mesh, transforms[meshCount], isStatic);
            entities.push_back(entity);

            /*
            if (meshCount == 1) {
                for (int32_t i = 0; i < 200000; i++) {
                    auto x = (2.0f * Atlas::Common::Random::SampleFastUniformFloat() - 1.0f) * 100.0f;
                    auto y = (2.0f * Atlas::Common::Random::SampleFastUniformFloat() - 1.0f) * 100.0f;
                    auto z = (2.0f * Atlas::Common::Random::SampleFastUniformFloat() - 1.0f) * 100.0f;

                    auto entity = scene->CreatePrefab<MeshInstance>(mesh, glm::translate(glm::mat4(1.0f),
                        glm::vec3(x, y, z)));
                    entities.push_back(entity);
                }
            }
            */

            meshCount++;
        }
    }

    scene->Timestep(1.0f);
    scene->Update();

    Atlas::Clock::ResetAverage();

    return successful;

}

void App::UnloadScene() {

    for (auto entity : entities) {
        scene->DestroyEntity(entity);
    }

    meshes.clear();
    entities.clear();

    meshes.shrink_to_fit();

    scene->ClearRTStructures();

    graphicsDevice->WaitForIdle();
    graphicsDevice->ForceMemoryCleanup();

}

void App::CheckLoadScene() {

    if (!scene->IsFullyLoaded() || loadingComplete)
        return;

    if (sceneSelection == NEWSPONZA) {
        for (auto& mesh : meshes) {
            mesh->data.colors.Clear();
        }
    }
    else if (sceneSelection == PICAPICA) {
        for (const auto& material : meshes.front()->data.materials)
            material->vertexColors = false;
    }
    else if (sceneSelection == EMERALDSQUARE) {
        for (const auto& mesh : meshes)
            for (const auto& material : mesh->data.materials)
                material->metalness = 0.0f;
    }

    graphicsDevice->WaitForPreviousFrameSubmission();

    auto sceneAABB = Atlas::Volume::AABB(glm::vec3(std::numeric_limits<float>::max()),
        glm::vec3(-std::numeric_limits<float>::max()));

    auto transformEntities = scene->GetSubset<MeshComponent>();
    for (auto entity : transformEntities) {
        const auto& comp = transformEntities.Get(entity);
        sceneAABB.Grow(comp.aabb);
    }

    for (auto& mesh : meshes) {
        if (!mesh.IsLoaded()) continue;
        mesh->invertUVs = true;
        mesh->cullBackFaces = true;
    }

    if (sceneSelection == CORNELL) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.10f), glm::ivec3(20));
        scene->irradianceVolume->sampleEmissives = true;
        scene->irradianceVolume->SetRayCount(512, 32);
    }
    else if (sceneSelection == SPONZA) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(0.9f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
        scene->irradianceVolume->strength = 1.5f;
    }
    else if (sceneSelection == BISTRO) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(0.9f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(32, 32);
        scene->irradianceVolume->strength = 1.5f;
    }
    else if (sceneSelection == SANMIGUEL) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.0f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == MEDIEVAL) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.0f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == PICAPICA) {
        for (auto& material : meshes.front()->data.materials) material->twoSided = false;

        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.0f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == SUBWAY) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == MATERIALS) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == FOREST) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(32, 32);
    }
    else if (sceneSelection == NEWSPONZA) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == EMERALDSQUARE) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }
    else if (sceneSelection == FLYINGWORLD) {
        scene->irradianceVolume = std::make_shared<Atlas::Lighting::IrradianceVolume>(
            sceneAABB.Scale(1.05f), glm::ivec3(20));
        scene->irradianceVolume->SetRayCount(128, 32);
    }

    scene->irradianceVolume->useShadowMap = true;
    scene->irradianceVolume->visibility = false;

    // Add rigid body components to entities (we need to wait for loading to complete to get valid mesh bounds)
    int32_t entityCount = 0;
    for (auto& entity : entities) {
        const auto& transformComponent = entity.GetComponent<TransformComponent>();
        const auto& meshComponent = entity.GetComponent<MeshComponent>();

        auto scale = transformComponent.Decompose().scale;
        if (entityCount++ == 0) {
            Atlas::Physics::MeshShapeSettings settings = {
                .mesh = meshComponent.mesh,
                .scale = scale
            };
            auto shape = Atlas::Physics::ShapesManager::CreateShape(settings);

            auto bodySettings = Atlas::Physics::BodyCreationSettings{
                .objectLayer = Atlas::Physics::Layers::Static,
                .shape = shape,
            };
            entity.AddComponent<RigidBodyComponent>(bodySettings);
        }
        else {
            Atlas::Physics::BoundingBoxShapeSettings settings = {
                .aabb = meshComponent.mesh->data.aabb,
                .scale = scale
            };
            auto shape = Atlas::Physics::ShapesManager::CreateShape(settings);

            auto bodySettings = Atlas::Physics::BodyCreationSettings{
                .objectLayer = Atlas::Physics::Layers::Static,
                .shape = shape,
            };
            entity.AddComponent<RigidBodyComponent>(bodySettings);
        }
    }

    scene->physicsWorld->OptimizeBroadphase();

    Atlas::Clock::ResetAverage();

    loadingComplete = true;

}

void App::SetResolution(int32_t width, int32_t height) {

    renderTarget->Resize(width, height);

}

void App::CopyActors(Atlas::Ref<Atlas::Scene::Scene> otherScene) {

    // Can use entity map for user created components to merge them as well
    scene->Merge(otherScene);

    auto otherMeshes = otherScene->GetMeshes();

    for (auto mesh : otherMeshes) {

        meshes.push_back(mesh);

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}
