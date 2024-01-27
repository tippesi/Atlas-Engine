#include "App.h"

#include <chrono>
#include <thread>

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
    .assetDirectory = "../../data",
    .shaderDirectory = "shader"
};

using namespace Atlas::Scene::Components;
using namespace Atlas::Scene::Prefabs;

void App::LoadContent() {

    music = Atlas::ResourceManager<Atlas::Audio::AudioData>::GetOrLoadResource("more.wav");
    audio = Atlas::ResourceManager<Atlas::Audio::AudioData>::GetOrLoadResource("more.wav");
    // static auto audioStream = Atlas::Audio::AudioManager::CreateStream(audio);

    for (uint32_t i = 0; i < 10000; i++) {
        //audioStreams.push_back(Atlas::Audio::AudioManager::CreateStream(audio));
        //audioStreams.back()->SetVolume(0.0001);
    }

    renderTarget = Atlas::CreateRef<Atlas::RenderTarget>(1920, 1080);
    pathTraceTarget = Atlas::CreateRef<Atlas::Renderer::PathTracerRenderTarget>(1920, 1080);

    viewport = Atlas::CreateRef<Atlas::Viewport>(0, 0, renderTarget->GetWidth(), renderTarget->GetHeight());

    auto icon = Atlas::Texture::Texture2D("icon.png");
    window.SetIcon(&icon);

    loadingTexture = Atlas::CreateRef<Atlas::Texture::Texture2D>("loading.png");

    font = Atlas::CreateRef<Atlas::Font>("font/roboto.ttf", 22.0f, 5);

    scene = Atlas::CreateRef<Atlas::Scene::Scene>("demoScene", glm::vec3(-2048.0f), glm::vec3(2048.0f));

    cameraEntity = scene->CreateEntity();
    auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 400.0f,
        glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));

    mouseHandler = Atlas::Input::MouseHandler(camera, 1.5f, 8.0f);
    keyboardHandler = Atlas::Input::KeyboardHandler(camera, 7.0f, 5.0f);
    controllerHandler = Atlas::Input::ControllerHandler(camera, 1.0f, 5.0f, 10.0f, 5000.0f);

    Atlas::Events::EventManager::KeyboardEventDelegate.Subscribe(
        [this](Atlas::Events::KeyboardEvent event) {
            if (event.keyCode == AE_KEY_ESCAPE) {
                Exit();
            }
            if (event.keyCode == AE_KEY_F11 && event.state == AE_BUTTON_RELEASED) {
                renderUI = !renderUI;
            }
            if (event.keyCode == AE_KEY_LSHIFT && event.state == AE_BUTTON_PRESSED) {
                keyboardHandler.speed = cameraSpeed * 4.0f;
            }
            if (event.keyCode == AE_KEY_LSHIFT && event.state == AE_BUTTON_RELEASED) {
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

    directionalLight = Atlas::CreateRef<Atlas::Lighting::DirectionalLight>(AE_MOVABLE_LIGHT);
    directionalLight->direction = glm::vec3(0.0f, -1.0f, 1.0f);
    directionalLight->color = glm::vec3(255, 236, 209) / 255.0f;
    glm::mat4 orthoProjection = glm::ortho(-100.0f, 100.0f, -70.0f, 120.0f, -120.0f, 120.0f);
    directionalLight->AddShadow(200.0f, 3.0f, 4096, glm::vec3(0.0f), orthoProjection);
    directionalLight->AddVolumetric(10, 0.28f);

    scene->sky.sun = directionalLight;

    scene->ao = Atlas::CreateRef<Atlas::Lighting::AO>(16);
    scene->ao->rt = true;
    // Use SSGI by default
    scene->ao->enable = false; 
    scene->reflection = Atlas::CreateRef<Atlas::Lighting::Reflection>(1);
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

    scene->physicsWorld = Atlas::CreateRef<Atlas::Physics::PhysicsWorld>();
    scene->rayTracingWorld = Atlas::CreateRef<Atlas::RayTracing::RayTracingWorld>();

    LoadScene();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    imguiWrapper.Load(&window);

}

void App::UnloadContent() {

    UnloadScene();
    imguiWrapper.Unload();

}

void App::Update(float deltaTime) {

    if (sceneReload) {
        UnloadScene();
        LoadScene();
        sceneReload = false;
    }

    const ImGuiIO& io = ImGui::GetIO();

    imguiWrapper.Update(&window, deltaTime);

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

    if(moveCamera) {
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

            if (meshComponent.mesh->name == "chromesphere.gltf") {
                float height = (sinf(Atlas::Clock::Get() / 5.0f) + 1.0f) * 20.0f;
                auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, height, .0f));

                transformComponent.Set(matrix);
            }
            else if (meshComponent.mesh->name == "metallicwall.gltf") {
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

            auto shape = Atlas::Physics::ShapesManager::CreateShapeFromSphere(meshes.back()->data.radius,
                glm::vec3(sphereScale), sphereDensity);
            auto& rigidBodyComponent = entity.AddComponent<RigidBodyComponent>(shape, Atlas::Physics::Layers::MOVABLE);
            rigidBodyComponent.SetRestitution(sphereRestitution);
            entity.AddComponent<AudioComponent>(audio);

            entities.push_back(entity);
            lastSpawn = Atlas::Clock::Get();
        }

    }

    if (scene->IsFullyLoaded() && shootSphere) {

        static float lastSpawn = 0.0f;
        

        if (Atlas::Clock::Get() - shootSpawnRate > lastSpawn) {
            auto shape = Atlas::Physics::ShapesManager::CreateShapeFromSphere(meshes.back()->data.radius,
                glm::vec3(sphereScale), sphereDensity);

            auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(camera.GetLocation() +
                camera.direction * meshes.back()->data.radius * 2.0f));
            auto entity = scene->CreatePrefab<MeshInstance>(meshes.back(), matrix, false);

            entity.AddComponent<AudioComponent>(audio);
            auto& rigidBodyComponent = entity.AddComponent<RigidBodyComponent>(shape, Atlas::Physics::Layers::MOVABLE);
            rigidBodyComponent.SetLinearVelocity(camera.direction * shootVelocity);
            rigidBodyComponent.SetMotionQuality(Atlas::Physics::MotionQuality::LinearCast);
            rigidBodyComponent.SetRestitution(sphereRestitution);

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
    static bool pathTrace = false;
    static bool debugAo = false;
    static bool debugReflection = false;
    static bool debugClouds = false;
    static bool debugSSS = false;
    static bool debugSSGI = false;
    static bool debugMotion = false;
    static bool slowMode = false;

    static float cloudDepthDebug = 0.0f;

#ifndef AE_HEADLESS
    auto windowFlags = window.GetFlags();
    if (windowFlags & AE_WINDOW_HIDDEN || windowFlags & AE_WINDOW_MINIMIZED || !(windowFlags & AE_WINDOW_SHOWN)) {
        return;
    }
#endif

    if (!loadingComplete) {
        DisplayLoadingScreen(deltaTime);
        return;
    }

    if (animateLight) directionalLight->direction = glm::vec3(0.0f, -1.0f, sin(Atlas::Clock::Get() / 10.0f));

    if (pathTrace) {
        viewport->Set(0, 0, pathTraceTarget->GetWidth(), pathTraceTarget->GetHeight());
        mainRenderer->PathTraceScene(viewport, pathTraceTarget, scene);
    }
    else {
        mainRenderer->RenderScene(viewport, renderTarget, scene);

        auto debug = debugAo || debugReflection || debugClouds || debugSSS || debugSSGI || debugMotion;

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
            else if (debugSSGI) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->giTexture,
                    0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
            }
            else if (debugMotion) {
                mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport,
                    renderTarget->GetData(Atlas::FULL_RES)->velocityTexture.get(),
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
        const auto& light = directionalLight;
        const auto& volume = scene->irradianceVolume;
        const auto& ao = scene->ao;
        const auto& fog = scene->fog;
        const auto& reflection = scene->reflection;
        const auto& clouds = scene->sky.clouds;
        const auto& sss = scene->sss;
        const auto& ssgi = scene->ssgi;
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
            if(pathTrace) ImGui::Text("Samples: %d", mainRenderer->pathTracingRenderer.GetSampleCount());
            ImGui::Text("Average frametime: %.3f ms", averageFramerate * 1000.0f);
            ImGui::Text("Current frametime: %.3f ms", deltaTime * 1000.0f);
            ImGui::Text("Camera location: %s", vecToString(camera.location).c_str());
            ImGui::Text("Scene dimensions: %s to %s", vecToString(sceneAABB.min).c_str(),vecToString(sceneAABB.max).c_str());
            ImGui::Text("Scene triangle count: %d", triangleCount);
            ImGui::Text("Number of entities: %zu", scene->GetEntityCount());

            {
                const char* items[] = { "Cornell box", "Sponza", "San Miguel",
                                        "New Sponza", "Bistro", "Medieval", "Pica Pica",
                                        "Subway", "Materials", "Forest", "Emerald square",
                                        "Flying world"};
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
                    graphicsDevice->CompleteFrame();
                    if (vsync) graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_FIFO_KHR, colorSpace);
                    else graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_IMMEDIATE_KHR, colorSpace);
                    vsyncMode = vsync;
                    hdrMode = hdr;
                    imguiWrapper.RecreateImGuiResources();
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
            }
            if (ImGui::CollapsingHeader("DDGI")) {
                ImGui::Text("Probe count: %s", vecToString(volume->probeCount).c_str());
                ImGui::Text("Cell size: %s", vecToString(volume->cellSize).c_str());
                ImGui::Checkbox("Enable volume##DDGI", &volume->enable);
                ImGui::Checkbox("Update volume##DDGI", &volume->update);
                ImGui::Checkbox("Visualize probes##DDGI", &volume->debug);
                ImGui::Checkbox("Sample emissives##DDGI", &volume->sampleEmissives);
                ImGui::Checkbox("Use shadow map##DDGI", &volume->useShadowMap);
                ImGui::Checkbox("Opacity check##DDGI", &volume->opacityCheck);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Uses the shadow map to calculate shadows in reflections. \
                        This is only possible when cascaded shadow maps are not used.");
                }

                const char* gridResItems [] = { "5x5x5", "10x10x10", "20x20x20", "30x30x30" };
                int currentItem = 0;
                if (volume->probeCount == glm::ivec3(5)) currentItem = 0;
                if (volume->probeCount == glm::ivec3(10)) currentItem = 1;
                if (volume->probeCount == glm::ivec3(20)) currentItem = 2;
                if (volume->probeCount == glm::ivec3(30)) currentItem = 3;
                auto prevItem = currentItem;
                ImGui::Combo("Resolution##DDGI", &currentItem, gridResItems, IM_ARRAYSIZE(gridResItems));

                if (currentItem != prevItem) {
                    switch (currentItem) {
                        case 0: volume->SetProbeCount(glm::ivec3(5)); break;
                        case 1: volume->SetProbeCount(glm::ivec3(10)); break;
                        case 2: volume->SetProbeCount(glm::ivec3(20)); break;
                        case 3: volume->SetProbeCount(glm::ivec3(30)); break;
                    }
                }

                const char* rayCountItems[] = { "32", "64", "128", "256", "512" };
                currentItem = 0;
                if (volume->rayCount == 32) currentItem = 0;
                if (volume->rayCount == 64) currentItem = 1;
                if (volume->rayCount == 128) currentItem = 2;
                if (volume->rayCount == 256) currentItem = 3;
                if (volume->rayCount == 512) currentItem = 4;
                prevItem = currentItem;
                ImGui::Combo("Ray count##DDGI", &currentItem, rayCountItems, IM_ARRAYSIZE(rayCountItems));

                if (currentItem != prevItem) {
                    switch (currentItem) {
                        case 0: volume->SetRayCount(32, 32); break;
                        case 1: volume->SetRayCount(64, 32); break;
                        case 2: volume->SetRayCount(128, 32); break;
                        case 3: volume->SetRayCount(256, 32); break;
                        case 4: volume->SetRayCount(512, 32); break;
                    }
                }

                ImGui::SliderFloat("Strength##DDGI", &volume->strength, 0.0f, 5.0f);
                ImGui::Separator();
                ImGui::Text("AABB");
                ImGui::SliderFloat3("Min", (float*)&volume->aabb.min, -200.0f, 200.0f);
                ImGui::SliderFloat3("Max", (float*)&volume->aabb.max, -200.0f, 200.0f);
                volume->SetAABB(volume->aabb);
                ImGui::Separator();
                ImGui::SliderFloat("Hysteresis", &volume->hysteresis, 0.0f, 1.0f, "%.3f");
                ImGui::SliderFloat("Sharpness", &volume->sharpness, 0.01f, 200.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Bias", &volume->bias, 0.0f, 1.0f);
                auto prevGamma = volume->gamma;
                ImGui::SliderFloat("Gamma exponent", &volume->gamma, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                if (prevGamma != volume->gamma) volume->ClearProbes();
                ImGui::Separator();
                if (ImGui::Button("Reset probe offsets")) {
                    volume->ResetProbeOffsets();
                }
                ImGui::Checkbox("Optimize probes", &volume->optimizeProbes);
            }
            if (ImGui::CollapsingHeader("Light")) {
                ImGui::Checkbox("Animate", &animateLight);
                ImGui::SliderFloat3("Direction", (float*)&light->direction, -1.0f, 1.0f);
                ImGui::ColorEdit3("Color", (float*)&light->color);
                ImGui::SliderFloat("Intensity##Light", &light->intensity, 0.0, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::Separator();
                ImGui::Text("Volumetric");
                ImGui::SliderFloat("Intensity##Volumetric", &light->GetVolumetric()->intensity, 0.0f, 1.0f);
                ImGui::Text("Shadow");
                auto shadow = light->GetShadow();
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
            }
            if (ImGui::CollapsingHeader("Screen-space shadows (preview)")) {
                ImGui::Checkbox("Debug##SSS", &debugSSS);
                ImGui::Checkbox("Enable##SSS", &sss->enable);
                ImGui::SliderInt("Sample count##SSS", &sss->sampleCount, 2.0, 16.0);
                ImGui::SliderFloat("Max length##SSS", &sss->maxLength, 0.01f, 1.0f);
                ImGui::SliderFloat("Thickness##SSS", &sss->thickness, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            }
            if (ImGui::CollapsingHeader("SSGI")) {
                ImGui::Checkbox("Debug##SSGI", &debugSSGI);
                ImGui::Checkbox("Enable##SSGI", &ssgi->enable);
                ImGui::Checkbox("Enable ambient occlusion##SSGI", &ssgi->enableAo);
                ImGui::SliderInt("Ray count##SSGI", &ssgi->rayCount, 1, 8);
                ImGui::SliderInt("Sample count##SSGI", &ssgi->sampleCount, 1, 16);
                ImGui::SliderFloat("Radius##SSGI", &ssgi->radius, 0.0f, 10.0f);
                ImGui::SliderFloat("Ao strength##SSGI", &ssgi->aoStrength, 0.0f, 10.0f);
                ImGui::SliderFloat("Irradiance limit##SSGI", &ssgi->irradianceLimit, 0.0f, 10.0f);
                //ImGui::SliderInt("Sample count##Ao", &ao->s, 0.0f, 20.0f, "%.3f", 2.0f);
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
            if (ImGui::CollapsingHeader("Reflection (preview)")) {
                ImGui::Checkbox("Debug##Reflection", &debugReflection);
                ImGui::Checkbox("Enable reflection", &reflection->enable);
                //ImGui::Checkbox("Enable raytracing##Reflection", &reflection->rt);
                ImGui::Checkbox("Use shadow map", &reflection->useShadowMap);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Uses the shadow map to calculate shadows in reflections. \
                        This is only possible when cascaded shadow maps are not used.");
                }
                ImGui::Checkbox("Enable GI in reflection", &reflection->gi);
                ImGui::Checkbox("Opacity check##Reflection", &reflection->opacityCheck);
                // ImGui::SliderInt("Sample count", &reflection->sampleCount, 1, 32);
                ImGui::SliderFloat("Radiance Limit##Reflection", &reflection->radianceLimit, 0.0f, 10.0f);
                ImGui::SliderFloat("Bias##Reflection", &reflection->bias, 0.0f, 1.0f);
                ImGui::SliderInt("Texture level##Reflection", &reflection->textureLevel, 0, 10);
                ImGui::Text("Denoiser");
                ImGui::SliderFloat("Spatial filter strength##Reflection", &reflection->spatialFilterStrength, 0.0f, 10.0f);
                ImGui::SliderFloat("Temporal weight##Reflection", &reflection->temporalWeight, 0.0f, 1.0f);
                ImGui::SliderFloat("Maximum history clip factor##Reflection", &reflection->historyClipMax, 0.0f, 1.0f);
                ImGui::SliderFloat("Current clip factor##Reflection", &reflection->currentClipFactor, 0.0f, 10.0f);
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
                ImGui::Checkbox("Enable##Fog", &fog->enable);
                fog->color = glm::pow(fog->color, 1.0f / glm::vec3(2.2f));
                ImGui::ColorEdit3("Color##Fog", &fog->color[0]);
                fog->color = glm::pow(fog->color, glm::vec3(2.2f));

                ImGui::SliderFloat("Density##Fog", &fog->density, 0.0f, 0.5f, "%.4f", 4.0f);
                ImGui::SliderFloat("Height##Fog", &fog->height, 0.0f, 300.0f, "%.3f", 4.0f);
                ImGui::SliderFloat("Height falloff##Fog", &fog->heightFalloff, 0.0f, 0.5f,
                    "%.4f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Scattering anisotropy##Fog", &fog->scatteringAnisotropy, -1.0f, 1.0f,
                    "%.3f", ImGuiSliderFlags_Logarithmic);
            }
            if (ImGui::CollapsingHeader("Clouds")) {
                ImGui::Checkbox("Enable##Clouds", &clouds->enable);
                ImGui::Checkbox("Cast shadow##Clouds", &clouds->castShadow);
                ImGui::Checkbox("Stochastic occlusion sampling##Clouds", &clouds->stochasticOcclusionSampling);
                ImGui::Checkbox("Debug##Clouds", &debugClouds);
                ImGui::Text("Quality");
                ImGui::SliderInt("Sample count##Clouds", &clouds->sampleCount, 1, 128);
                ImGui::SliderInt("Shadow sample count##Clouds", &clouds->occlusionSampleCount, 1, 16);
                ImGui::SliderInt("Shadow sample fraction count##Clouds", &clouds->shadowSampleFraction, 1, 4);
                ImGui::Text("Shape");
                ImGui::SliderFloat("Density multiplier##Clouds", &clouds->densityMultiplier, 0.0f, 1.0f);
                ImGui::SliderFloat("Height stretch##Clouds", &clouds->heightStretch, 0.0f, 1.0f);
                if (ImGui::Button("Update noise textures##Clouds")) {
                    clouds->needsNoiseUpdate = true;
                }
                ImGui::Separator();
                ImGui::Text("Dimensions");
                ImGui::SliderFloat("Min height##Clouds", &clouds->minHeight, 0.0f, 2000.0f);
                ImGui::SliderFloat("Max height##Clouds", &clouds->maxHeight, 0.0f, 4000.0f);
                ImGui::SliderFloat("GetDistance limit##Clouds", &clouds->distanceLimit, 0.0f, 10000.0f);
                ImGui::Separator();
                ImGui::Text("Scattering");
                ImGui::ColorPicker3("Extinction coefficients", &clouds->scattering.extinctionCoefficients[0]);
                ImGui::SliderFloat("Extinction factor", &clouds->scattering.extinctionFactor, 0.0001f, 10.0f);
                ImGui::SliderFloat("Scattering factor", &clouds->scattering.scatteringFactor, 0.0001f, 10.0f);
                ImGui::SliderFloat("Eccentricity first phase", &clouds->scattering.eccentricityFirstPhase, -1.0f, 1.0f);
                ImGui::SliderFloat("Eccentricity second phase", &clouds->scattering.eccentricitySecondPhase, -1.0f, 1.0f);
                ImGui::SliderFloat("Phase alpha", &clouds->scattering.phaseAlpha, 0.0f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Noise texture behaviour");
                ImGui::SliderFloat("Shape scale##Clouds", &clouds->shapeScale, 0.0f, 100.0f);
                ImGui::SliderFloat("Detail scale##Clouds", &clouds->detailScale, 0.0f, 100.0f);
                ImGui::SliderFloat("Shape speed##Clouds", &clouds->shapeSpeed, 0.0f, 10.0f);
                ImGui::SliderFloat("Detail speed##Clouds", &clouds->detailSpeed, 0.0f, 10.0f);
                ImGui::SliderFloat("Detail strength##Clouds", &clouds->detailStrength, 0.0f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Silver lining");
                ImGui::SliderFloat("Dark edge strength##Clouds", &clouds->darkEdgeFocus, 0.0f, 1025.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Dark edge ambient##Clouds", &clouds->darkEdgeAmbient, 0.0f, 1.0f);
            }
            if (ImGui::CollapsingHeader("Postprocessing")) {
                ImGui::Text("Temporal anti-aliasing");
                ImGui::Checkbox("Enable##TAA", &postProcessing.taa.enable);
                ImGui::Checkbox("Enable slow mode##SlowMode", &slowMode);
                ImGui::SliderFloat("Jitter range##TAA", &postProcessing.taa.jitterRange, 0.001f, 0.999f);
                ImGui::Separator();
                ImGui::Text("Sharpen filter");
                ImGui::Checkbox("Enable##Sharpen", &postProcessing.sharpen.enable);
                ImGui::SliderFloat("Sharpness", &postProcessing.sharpen.factor, 0.0f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Image effects");
                ImGui::Checkbox("Filmic tonemapping", &postProcessing.filmicTonemapping);
                ImGui::SliderFloat("Saturation##Postprocessing", &postProcessing.saturation, 0.0f, 2.0f);
                ImGui::SliderFloat("Contrast##Postprocessing", &postProcessing.contrast, 0.0f, 2.0f);
                ImGui::SliderFloat("White point##Postprocessing", &postProcessing.whitePoint, 0.0f, 100.0f, "%.3f", 2.0f);
                ImGui::Separator();
                ImGui::Text("Chromatic aberration");
                ImGui::Checkbox("Enable##Chromatic aberration", &postProcessing.chromaticAberration.enable);
                ImGui::Checkbox("Colors reversed##Chromatic aberration", &postProcessing.chromaticAberration.colorsReversed);
                ImGui::SliderFloat("Strength##Chromatic aberration", &postProcessing.chromaticAberration.strength, 0.0f, 4.0f);
                ImGui::Text("Film grain");
                ImGui::Checkbox("Enable##Film grain", &postProcessing.filmGrain.enable);
                ImGui::SliderFloat("Strength##Film grain", &postProcessing.filmGrain.strength, 0.0f, 1.0f);
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
            }
            if (ImGui::CollapsingHeader("Materials")) {
                int32_t id = 0;
                auto materials = scene->GetMaterials();
                for (auto material : materials) {
                    auto label = material->name + "##mat" + std::to_string(id++);

                    if (ImGui::TreeNode(label.c_str())) {
                        auto twoSidedLabel = "Two sided##" + label;
                        auto baseColorLabel = "Base color##" + label;
                        auto emissionColorLabel = "Emission color##" + label;
                        auto emissionPowerLabel = "Emission power##" + label;
                        auto transmissionColorLabel = "Transmission color##" + label;

                        ImGui::Checkbox(twoSidedLabel.c_str(), &material->twoSided);
                        ImGui::ColorEdit3(baseColorLabel.c_str(), glm::value_ptr(material->baseColor));
                        ImGui::ColorEdit3(emissionColorLabel.c_str(), glm::value_ptr(material->emissiveColor));
                        ImGui::SliderFloat(emissionPowerLabel.c_str(), &material->emissiveIntensity, 1.0f, 10000.0f,
                            "%.3f", ImGuiSliderFlags_Logarithmic);

                        auto roughnessLabel = "Roughness##" + label;
                        auto metallicLabel = "Metallic##" + label;
                        auto reflectanceLabel = "Reflectance##" + label;
                        auto aoLabel = "Ao##" + label;
                        auto opacityLabel = "Opacity##" + label;

                        ImGui::SliderFloat(roughnessLabel.c_str(), &material->roughness, 0.0f, 1.0f);
                        ImGui::SliderFloat(metallicLabel.c_str(), &material->metalness, 0.0f, 1.0f);
                        ImGui::SliderFloat(reflectanceLabel.c_str(), &material->reflectance, 0.0f, 1.0f);
                        ImGui::SliderFloat(aoLabel.c_str(), &material->ao, 0.0f, 1.0f);
                        ImGui::SliderFloat(opacityLabel.c_str(), &material->opacity, 0.0f, 1.0f);

                        ImGui::TreePop();
                    }
                }
            }
            if (ImGui::CollapsingHeader("Controls")) {
                ImGui::Text("Use WASD for movement");
                ImGui::Text("Use left mouse click + mouse movement to look around");
                ImGui::Text("Use F11 to hide/unhide the UI");
            }
            if (ImGui::CollapsingHeader("Profiler")) {
                bool enabled = Atlas::Graphics::Profiler::enable;
                ImGui::Checkbox("Enable##Profiler", &enabled);
                Atlas::Graphics::Profiler::enable = enabled;

                const char* items[] = { "Chronologically", "Max time", "Min time" };
                static int item = 0;
                ImGui::Combo("Sort##Performance", &item, items, IM_ARRAYSIZE(items));

                Atlas::Graphics::Profiler::OrderBy order;
                switch (item) {
                    case 1: order = Atlas::Graphics::Profiler::OrderBy::MAX_TIME; break;
                    case 2: order = Atlas::Graphics::Profiler::OrderBy::MIN_TIME; break;
                    default: order = Atlas::Graphics::Profiler::OrderBy::CHRONO; break;
                }

                std::function<void(Atlas::Graphics::Profiler::Query&)> displayQuery;
                displayQuery = [&displayQuery](Atlas::Graphics::Profiler::Query& query) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGuiTreeNodeFlags expandable = 0;
                    if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                             ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                    bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::TableNextColumn();
                    ImGui::Text("%f", double(query.timer.elapsedTime) / 1000000.0);
                    // ImGui::TableNextColumn();
                    // ImGui::TextUnformatted(node->Type);

                    if (open && query.children.size()) {
                        for (auto& child : query.children)
                            displayQuery(child);
                        ImGui::TreePop();
                    }

                };

                static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

                if (ImGui::BeginTable("PerfTable", 2, flags))
                {
                    // The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("Elapsed (ms)", ImGuiTableColumnFlags_NoHide);
                    //ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableHeadersRow();

                    auto threadData = Atlas::Graphics::Profiler::GetQueriesAverage(64, order);
                    for (auto& thread : threadData) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGuiTreeNodeFlags expandable = 0;
                        if (!thread.queries.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                                 ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                        bool open = ImGui::TreeNodeEx(thread.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);

                        if (open && thread.queries.size()) {
                            for (auto &query: thread.queries)
                                displayQuery(query);
                            ImGui::TreePop();
                        }
                    }


                    ImGui::EndTable();
                }
            }

            ImGui::End();
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

#ifdef AE_HEADLESS
        Atlas::Log::Message("Frame rendererd");
        renderTarget.hdrTexture.Save<float>("prepost");
        renderTarget.postProcessTexture.Save<uint8_t>("result");
#endif

        if (!recreateSwapchain) {
            imguiWrapper.Render();
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

}

void App::DisplayLoadingScreen(float deltaTime) {

    auto commandList = graphicsDevice->GetCommandList();

    commandList->BeginCommands();
    graphicsDevice->swapChain->colorClearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
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

    Atlas::Texture::Cubemap sky;
    directionalLight->direction = glm::vec3(0.0f, -1.0f, 1.0f);

    auto& camera = cameraEntity.GetComponent<CameraComponent>();

    scene->sky.clouds = Atlas::CreateRef<Atlas::Lighting::VolumetricClouds>();
    scene->sky.clouds->minHeight = 1400.0f;
    scene->sky.clouds->maxHeight = 1700.0f;
    scene->sky.clouds->castShadow = false;

    scene->sky.probe = nullptr;
    scene->sky.clouds->enable = true;
    scene->sss->enable = true;

    using namespace Atlas::Loader;

    if (sceneSelection == CORNELL) {
        meshes.reserve(1);

        glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "cornell/CornellBox-Original.obj", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 0.0f;
        directionalLight->GetVolumetric()->intensity = 0.0f;

        // Setup camera
        camera.location = glm::vec3(0.0f, 14.0f, 40.0f);
        camera.rotation = glm::vec2(-3.14f, -0.1f);

        scene->fog->enable = false;
    }
    else if (sceneSelection == SPONZA) {
        meshes.reserve(1);

        glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(.05f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "sponza/sponza.obj", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);
 
        transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "metallicwall.gltf", ModelLoader::LoadMesh, Atlas::Mesh::MeshMobility::Movable,
            false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->direction = glm::vec3(0.0f, -1.0f, 0.33f);
        directionalLight->intensity = 100.0f;
        directionalLight->GetVolumetric()->intensity = 0.28f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 0.125f;

        scene->fog->enable = true;
    }
    else if (sceneSelection == BISTRO) {
        meshes.reserve(1);

        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(.015f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "bistro/mesh/exterior.obj", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 100.0f;
        directionalLight->GetVolumetric()->intensity = 0.28f;

        // Setup camera
        camera.location = glm::vec3(-21.0f, 8.0f, 1.0f);
        camera.rotation = glm::vec2(3.14f / 2.0f, 0.0f);
        camera.exposure = 0.125f;

        scene->fog->enable = true;
    }
    else if (sceneSelection == SANMIGUEL) {
        meshes.reserve(1);

        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "sanmiguel/san-miguel-low-poly.obj", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 100.0f;
        directionalLight->GetVolumetric()->intensity = 0.28f;
        directionalLight->direction = glm::vec3(0.0f, -1.0f, -1.0f);

        // Setup camera
        camera.location = glm::vec3(45.0f, 26.0f, 17.0f);
        camera.rotation = glm::vec2(-4.14f / 2.0f, -.6f);
        camera.exposure = 2.5f;

        scene->fog->enable = true;
    }
    else if (sceneSelection == MEDIEVAL) {
        meshes.reserve(1);

        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "medieval/scene.fbx", ModelLoader::LoadMesh, false, glm::mat4(1.0f), 2048
        );
        meshes.push_back(mesh);

        // Metalness is set to 0.9f
        //for (auto& material : mesh.data.materials) material.metalness = 0.0f;

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 10.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

        scene->fog->enable = true;
    }
    else if (sceneSelection == PICAPICA) {
        meshes.reserve(1);

        auto transform = glm::rotate(glm::mat4(1.0f), -3.14f / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "pica pica/mesh/scene.gltf", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 10.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = true;
    }
    else if (sceneSelection == SUBWAY) {
        meshes.reserve(1);

        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "subway/scene.gltf", ModelLoader::LoadMesh, false, glm::mat4(1.0f), 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 10.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
    }
    else if (sceneSelection == MATERIALS) {
        meshes.reserve(1);

        auto transform = glm::scale(glm::vec3(8.0f));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "material demo/materials.obj", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        sky = Atlas::Texture::Cubemap("environment.hdr", 2048);
        probe = Atlas::Lighting::EnvironmentProbe(sky);
        scene->sky.probe = Atlas::CreateRef(probe);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 10.0f;
        directionalLight->GetVolumetric()->intensity = 0.0f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
        scene->sky.clouds->enable = false;
        scene->sss->enable = true;
    }
    else if (sceneSelection == FOREST) {
        auto otherScene = Atlas::Loader::ModelLoader::LoadScene("forest/forest.gltf");
        otherScene->Timestep(1.0f);

        CopyActors(otherScene);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 50.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
    }
    else if (sceneSelection == EMERALDSQUARE) {
        auto otherScene = Atlas::Loader::ModelLoader::LoadScene("emeraldsquare/square.gltf", false, glm::mat4(1.0f), 1024);
        otherScene->Timestep(1.0f);

        CopyActors(otherScene);

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 10.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->fog->enable = false;
    }
    else if (sceneSelection == FLYINGWORLD) {
        meshes.reserve(1);

        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "flying world/scene.gltf", ModelLoader::LoadMesh, false, glm::mat4(0.01f), 2048
        );
        meshes.push_back(mesh);

        // Metalness is set to 0.9f
        //for (auto& material : mesh.data.materials) material.metalness = 0.0f;

        // Other scene related settings apart from the mesh
        directionalLight->intensity = 50.0f;
        directionalLight->GetVolumetric()->intensity = 0.08f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);
        camera.exposure = 1.0f;

        scene->sky.clouds->minHeight = 700.0f;
        scene->sky.clouds->maxHeight = 1000.0f;
        scene->sky.clouds->densityMultiplier = 0.65f;
        scene->sky.clouds->heightStretch = 1.0f;

        scene->fog->enable = true;
    }
    else if (sceneSelection == NEWSPONZA) {
        meshes.reserve(4);

        auto transform = glm::mat4(glm::scale(glm::mat4(1.0f), glm::vec3(4.0f)));
        auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/main/NewSponza_Main_Blender_glTF.gltf", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/candles/NewSponza_100sOfCandles_glTF_OmniLights.gltf", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/curtains/NewSponza_Curtains_glTF.gltf", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);
        mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
            "newsponza/ivy/NewSponza_IvyGrowth_glTF.gltf", ModelLoader::LoadMesh, false, transform, 2048
        );
        meshes.push_back(mesh);

        // Other scene related settings apart from the mesh
        directionalLight->direction = glm::vec3(0.0f, -1.0f, 0.33f);
        directionalLight->intensity = 100.0f;
        directionalLight->GetVolumetric()->intensity = 0.28f;

        // Setup camera
        camera.location = glm::vec3(30.0f, 25.0f, 0.0f);
        camera.rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

        scene->fog->enable = true;
    }

    // scene.sky.probe = std::make_shared<Atlas::Lighting::EnvironmentProbe>(sky);

    // Load chrome sphere for every scene in order to test physics
    auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.f));
    auto mesh = Atlas::ResourceManager<Atlas::Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
        "chromesphere.gltf", ModelLoader::LoadMesh, Atlas::Mesh::MeshMobility::Movable,
        false, transform, 2048
    );
    meshes.push_back(mesh);

    if (sceneSelection != FOREST && sceneSelection != EMERALDSQUARE) {
        auto meshCount = 0;
        for (auto &mesh: meshes) {
            // Only Sponza scene gets extra moving ball
            if (mesh.GetID() == meshes.back().GetID() && sceneSelection != SPONZA)
                continue;

            auto isStatic = meshCount == 0;
            auto entity = scene->CreatePrefab<MeshInstance>(mesh, glm::mat4(1.0f), isStatic);
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

    static std::future<void> future;

    auto buildRTStructure = [&]() {
        auto sceneMeshes = scene->GetMeshes();

        for (const auto& mesh : sceneMeshes) {
            mesh->BuildBVH();
        }
    };

    if (!future.valid()) {
        future = std::async(std::launch::async, buildRTStructure);
        return;
    }
    else {
        if (future.wait_for(std::chrono::microseconds(0)) != std::future_status::ready) {
            return;
        }
        future.get();
    }

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

    // Add rigid body components to entities (we need to wait for loading to complete to get valid mesh bounds)
    int32_t entityCount = 0;
    for (auto& entity : entities) {
        auto& meshComponent = entity.GetComponent<MeshComponent>();
        if (entityCount++ == 0) {
            auto shape = Atlas::Physics::ShapesManager::CreateShapeFromMesh(meshComponent.mesh.Get());
            entity.AddComponent<RigidBodyComponent>(shape, Atlas::Physics::Layers::STATIC);
        }
        else {
            auto shape = Atlas::Physics::ShapesManager::CreateShapeFromAABB(meshComponent.mesh->data.aabb);
            entity.AddComponent<RigidBodyComponent>(shape, Atlas::Physics::Layers::STATIC);
        }
    }

    scene->physicsWorld->OptimizeBroadphase();

    Atlas::Clock::ResetAverage();

    loadingComplete = true;

}

void App::SetResolution(int32_t width, int32_t height) {

    renderTarget->Resize(width, height);
    pathTraceTarget->Resize(width, height);

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
