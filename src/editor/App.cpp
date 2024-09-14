#include "App.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Serialization.h"
#include "Notifications.h"
#include "ContentDiscovery.h"
#include "ui/panels/PopupPanels.h"
#include "tools/FileSystemHelper.h"
#include <ImGuizmo.h>
#include <implot.h>

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

namespace Atlas::Editor {

    void App::LoadContent() {

        ContentDiscovery::Update();

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        SetDefaultWindowResolution();

        auto icon = Atlas::Texture::Texture2D("icon.png");
        window.SetIcon(&icon);

        // Add font with enlarged size and scale it down again
        // This means we can use scaled text up to 2x the size
        io.Fonts->AddFontFromFileTTF(
            Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(),
            32.0f
        )->Scale = 0.5f;

        SubscribeToResourceEvents();

        Singletons::imguiWrapper = CreateRef<ImguiExtension::ImguiWrapper>();
        Singletons::imguiWrapper->Load(&window);
        Singletons::config = CreateRef<Config>();

        Serialization::DeserializeConfig();

        // Everything that needs the config comes afterwards
        Singletons::icons = CreateRef<Icons>();
        Singletons::blockingOperation = CreateRef<BlockingOperation>();
        Singletons::renderTarget = CreateRef<Renderer::RenderTarget>(1280, 720);
        Singletons::mainRenderer = mainRenderer;

        mouseHandler = Input::MouseHandler(1.5f, 8.0f);
        keyboardHandler = Input::KeyboardHandler(7.0f, 5.0f);

        if (Singletons::config->darkMode) {
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
        }
        else {
            ImGui::StyleColorsLight();
            ImPlot::StyleColorsLight();
        }

    }

    void App::UnloadContent() {

        Singletons::imguiWrapper->Unload();

        CopyPasteHelper::Clear();

        for (const auto& sceneWindow : sceneWindows) {
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            Serialization::SerializeSceneWindow(sceneWindow);
        }

        Serialization::SerializeConfig();
        ContentDiscovery::Shutdown();

        Singletons::Destruct();

        ImPlot::DestroyContext();

    }

    void App::Update(float deltaTime) {

        const ImGuiIO &io = ImGui::GetIO();

        ContentDiscovery::Update();
        Singletons::imguiWrapper->Update(&window, deltaTime);

        Singletons::blockingOperation->Update();

        if (Singletons::blockingOperation->block)
            return;

        contentBrowserWindow.Update();

        // Create new windows for fully loaded scenes
        for (size_t i = 0; i < waitToLoadScenes.size(); i++) {
            if (waitToLoadScenes[i].IsLoaded()) {
                std::swap(waitToLoadScenes[i], waitToLoadScenes.back());

                // After the scene is loaded we can retrieve the window configuration and deserialize it
                sceneWindows.emplace_back(Serialization::DeserializeSceneWindow(waitToLoadScenes.back()));
                waitToLoadScenes.pop_back();
                i--;
            }
        }

        // Quick quit
        if (sceneWindows.empty())
            return;

        auto& config = Singletons::config;

        std::vector<size_t> toBeDeletedSceneWindows;

        // Find active scene and old scene windows
        size_t sceneCounter = 0;
        for (const auto& sceneWindow : sceneWindows) {

            // Delete windows not opened in the config (don't know when this does happen)
            if (std::find(config->openedScenes.begin(), config->openedScenes.end(),
                sceneWindow->scene) == config->openedScenes.end()) {
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            // Delete closed windows
            if (!sceneWindow->show) {
                auto iter = std::find(config->openedScenes.begin(), config->openedScenes.end(),
                    sceneWindow->scene);
                config->openedScenes.erase(iter);
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            if (sceneWindow->inFocus)
                activeSceneIdx = sceneCounter;

            sceneCounter++;
        }

        // Reverse such that indices remain valid
        std::reverse(toBeDeletedSceneWindows.begin(), toBeDeletedSceneWindows.end());
        for (auto sceneWindowIdx : toBeDeletedSceneWindows) {
            // Serialize window before erasing it
            if (sceneWindows[sceneWindowIdx]->isPlaying)
                sceneWindows[sceneWindowIdx]->StopPlaying();
            Serialization::SerializeSceneWindow(sceneWindows[sceneWindowIdx]);
            sceneWindows.erase(sceneWindows.begin() + sceneWindowIdx);
        }

        // Avoid crash when nothing is there anymore
        if (sceneWindows.empty())
            return;

        // Update active scene with input
        activeSceneIdx = std::min(activeSceneIdx, sceneWindows.size() - 1);

        auto& activeSceneWindow = sceneWindows[activeSceneIdx];

        auto lockMovement = io.KeysDown[ImGuiKey_LeftCtrl] || io.KeysDown[ImGuiKey_LeftSuper];

        auto cameraEntity = activeSceneWindow->cameraEntity;
        if (cameraEntity.IsValid() && activeSceneWindow->viewportPanel.isFocused && !lockMovement &&
            !ImGuizmo::IsUsing() && !activeSceneWindow->isPlaying) {
            auto& camera = cameraEntity.GetComponent<CameraComponent>();
            mouseHandler.sensibility = activeSceneWindow->cameraRotationSpeed;
            keyboardHandler.speed = activeSceneWindow->cameraMovementSpeed;

            mouseHandler.Update(camera, deltaTime);
            keyboardHandler.Update(camera, deltaTime);
        }

        if (activeSceneWindow->isPlaying) {
            auto playerSubset = activeSceneWindow->scene->GetSubset<PlayerComponent>();
            for (auto entity : playerSubset) {
                auto camera = entity.TryGetComponent<CameraComponent>();
                if (!camera)
                    continue;

                auto& player = entity.GetComponent<PlayerComponent>();

                mouseHandler.Update(*camera, deltaTime);
                keyboardHandler.Update(*camera, player, deltaTime);
            }
        }

        // Update all scenes after input was applied
        for (auto& sceneWindow : sceneWindows) {
            if (sceneWindow == activeSceneWindow)
                sceneWindow->isActiveWindow = true;
            else
                sceneWindow->isActiveWindow = false;
            
            // Need to reset this each frame in order to reenable selection
            sceneWindow->lockSelection = false;
            sceneWindow->Update(deltaTime);
        }

        ImGuizmo::Enable(activeSceneWindow->needGuizmoEnabled);
        
    }

    void App::Render(float deltaTime) {

        graphicsDevice->WaitForPreviousFrameSubmission();

        auto windowFlags = window.GetFlags();
        if (windowFlags & AE_WINDOW_HIDDEN || windowFlags & AE_WINDOW_MINIMIZED || !(windowFlags & AE_WINDOW_SHOWN)) {
            return;
        }

        auto& config = Singletons::config;

        auto swapChainVsync = graphicsDevice->swapChain->presentMode == VK_PRESENT_MODE_FIFO_KHR;
        if (swapChainVsync != config->vsync) {
            if (config->vsync) graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_FIFO_KHR, Atlas::Graphics::SRGB_NONLINEAR);
            else graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_IMMEDIATE_KHR, Atlas::Graphics::SRGB_NONLINEAR);
        }

        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // ImGui::ShowDemoWindow();
        // ImPlot::ShowDemoWindow();

        ImGuiViewport *viewport = ImGui::GetMainViewport();

        bool playingMaximized = false;
        auto activeSceneWindow = sceneWindows.empty() ? nullptr : sceneWindows[activeSceneIdx];
        if (activeSceneWindow) {
            playingMaximized = activeSceneWindow->isPlaying && activeSceneWindow->playMaximized;
        }
        if (!playingMaximized) {
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::SetNextWindowBgAlpha(0.0f);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("MainDockspace Window", nullptr, window_flags);
            ImGui::PopStyleVar(3);

            ImGuiID mainDsId = ImGui::GetID("MainDS");
            if (!ImGui::DockBuilderGetNode(mainDsId) || resetDockspaceLayout) {
                SetupMainDockspace(mainDsId);
            }

            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
            ImGui::DockSpace(mainDsId, ImVec2(0.0f, 0.0f), dockspace_flags);

            if (ImGui::BeginMainMenuBar()) {
                static bool openProject = false, saveProject = false, newScene = false, importFiles = false;
                bool saveScene = false, exitEditor = false;
                if (ImGui::BeginMenu("File")) {
                    /*
                    ImGui::MenuItem("Open project", nullptr, &openProject);
                    ImGui::MenuItem("Save project", nullptr, &saveProject);
                    ImGui::Separator();
                    */
                    ImGui::MenuItem("New scene", nullptr, &newScene);
                    ImGui::MenuItem("Save scene", nullptr, &saveScene);
                    ImGui::MenuItem("Exit", nullptr, &exitEditor);
                    /*
                    ImGui::Separator();
                    ImGui::MenuItem("Import files", nullptr, &importFiles);
                    */
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    if (ImGui::MenuItem("Dark mode", nullptr, &config->darkMode)) {
                        if (config->darkMode) {
                            ImGui::StyleColorsDark();
                            ImPlot::StyleColorsDark();
                        }
                        else {
                            ImGui::StyleColorsLight();
                            ImPlot::StyleColorsLight();
                        }
                    }

                    ImGui::MenuItem("Reset layout", nullptr, &resetDockspaceLayout);
                    ImGui::MenuItem("Show logs", nullptr, &logWindow.show);
                    ImGui::MenuItem("Show content browser", nullptr, &contentBrowserWindow.show);
                    ImGui::MenuItem("Show profiler", nullptr, &profilerWindow.show);
                    ImGui::MenuItem("Show geometry brush", nullptr, &geometryBrushWindow.show);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Renderer")) {
                    ImGui::MenuItem("VSync", nullptr, &config->vsync);
                    ImGui::MenuItem("Pathtracer", nullptr, &config->pathTrace);
                    ImGui::EndMenu();
                }

                if (newScene) {
                    UI::PopupPanels::isNewScenePopupVisible = true;
                    newScene = false;
                }

                if (saveScene) {
                    auto activeSceneWindow = sceneWindows.empty() ? nullptr : sceneWindows[activeSceneIdx];
                    if (activeSceneWindow != nullptr)
                        activeSceneWindow->SaveScene();                      
                }

                if (exitEditor)
                    Exit();

                ImGui::EndMainMenuBar();
            }

            UI::PopupPanels::Render();

            geometryBrushWindow.Render(activeSceneWindow);

            for (auto& sceneWindow : sceneWindows) {
                sceneWindow->Render();
            }

            contentBrowserWindow.Render();
            logWindow.Render();
            profilerWindow.Render();

            ImGui::End();
        }
        else {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("RenderWindow", nullptr, window_flags);

            auto renderArea = ImGui::GetContentRegionAvail();
            activeSceneWindow->viewportPanel.RenderScene(activeSceneWindow->scene.Get(), 
                ivec2(0), ivec2(int32_t(renderArea.x), int32_t(renderArea.y)), true);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&activeSceneWindow->viewportPanel.viewportTexture);
            ImGui::Image(set, renderArea);

            if (activeSceneWindow->perfOverlayMaximized) {
                ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
                auto gpuProfilerData = Graphics::Profiler::GetQueriesAverage(32, Graphics::Profiler::OrderBy::MAX_TIME);

                std::string perfString;
                int32_t slowestThreadIdx = 0;
                double slowestTime = 0.0;
                for (int32_t i = 0; i < int32_t(gpuProfilerData.size()); i++) {
                    const auto& threadData = gpuProfilerData[i];
                    double threadTime = 0.0;
                    for (const auto& query : threadData.queries) {
                        threadTime += query.timer.elapsedTime;
                    }
                    if (threadTime > slowestTime) {
                        slowestTime = threadTime;
                        slowestThreadIdx = i;
                    }
                }

                auto target = Singletons::renderTarget;

                ImGui::SetWindowFontScale(1.5f);
                ImGui::Text("Frame time: %.3fms, GPU time: %.3f", Clock::GetAverage() * 1000.0f, float(slowestTime / 1000000.0));
                ImGui::Text("Resolution %dx%d upscaled from %dx%d", target->GetWidth(), target->GetHeight(), target->GetScaledWidth(), target->GetScaledHeight());
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();

            ImGui::End();
        }

        Notifications::Display();

        ImGui::Render();
        Singletons::imguiWrapper->Render(true);

    }

    void App::SetupMainDockspace(ImGuiID dsID) {

        ImGui::SetWindowSize(ImVec2(370.0f, 680.0f));
        ImGui::SetWindowPos(ImVec2(640.0f, 55.0f));

        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID);

        ImGuiID dsIdUp, dsIdDown;
        ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Up, 0.7f, &dsIdUp, &dsIdDown);

        for (auto& sceneWindow : sceneWindows) {
            ImGui::DockBuilderDockWindow(sceneWindow->GetNameID(), dsIdUp);
        }
        upperDockNodeID = dsIdUp;

        {
            auto dsIdCopy = dsIdDown;
            auto dsIdLeft = ImGui::DockBuilderSplitNode(dsIdCopy, ImGuiDir_Left, 0.5f, NULL, &dsIdCopy);
            auto dsIdRight = ImGui::DockBuilderSplitNode(dsIdCopy, ImGuiDir_Right, 0.0f, NULL, &dsIdCopy);

            ImGui::DockBuilderDockWindow(contentBrowserWindow.GetNameID(), dsIdLeft);
            ImGui::DockBuilderDockWindow(logWindow.GetNameID(), dsIdRight);
        }

        ImGui::DockBuilderFinish(dsID);

        profilerWindow.show = false;
        contentBrowserWindow.show = true;
        logWindow.show = true;

        contentBrowserWindow.resetDockingLayout = true;
        logWindow.resetDockingLayout = true;
        for (const auto& sceneWindow : sceneWindows) {
            sceneWindow->resetDockingLayout = true;
            sceneWindow->show = true;
        }

        resetDockspaceLayout = false;

    }

    void App::SubscribeToResourceEvents() {

        ResourceManager<Scene::Scene>::Subscribe(ResourceTopic::ResourceCreate,
            [&](Ref<Resource<Scene::Scene>>& scene) {
            waitToLoadScenes.push_back(ResourceHandle<Scene::Scene>(scene));
            Singletons::config->openedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
        });

        // Also kind of a resource event
        Events::EventManager::DropEventDelegate.Subscribe([&](Events::DropEvent event) {
            if (!event.file.empty() && contentBrowserWindow.show) {
                auto destinationDirectory = Common::Path::GetAbsolute(contentBrowserWindow.currentDirectory);

                FileSystemHelper::Copy(event.file, destinationDirectory);
            }
        });

    }

    void App::SetDefaultWindowResolution() {

        auto resolution = GetScreenSize();

        window.SetSize(resolution.x - 200, resolution.y - 200);
        window.SetPosition(100, 100);

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new Atlas::Editor::App();

}
