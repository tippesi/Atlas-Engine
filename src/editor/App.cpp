#include "App.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Serialization.h"
#include "Notifications.h"
#include "ContentDiscovery.h"
#include "Theme.h"
#include "ui/panels/PopupPanels.h"
#include "tools/FileSystemHelper.h"
#include <ImGuizmo.h>
#include <imgui_internal.h>

#include <filesystem>
#include <algorithm>
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
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
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
        Serialization::DeserializeContentBrowserWindow(contentBrowserWindow);

        // Everything that needs the config comes afterwards
        Singletons::icons = CreateRef<Icons>();
        Singletons::blockingOperation = CreateRef<BlockingOperation>();
        Singletons::renderTarget = CreateRef<Renderer::RenderTarget>(1280, 720);
        Singletons::mainRenderer = mainRenderer;

        mouseHandler = Input::MouseHandler(1.5f, 8.0f);
        keyboardHandler = Input::KeyboardHandler(7.0f, 5.0f);

        Theme::Apply(Singletons::config->darkMode);

        RegisterCommands();

    }

    void App::UnloadContent() {

        Singletons::imguiWrapper->Unload();

        CopyPasteHelper::Clear();

        for (const auto& sceneWindow : sceneWindows) {
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            Serialization::SerializeSceneWindow(sceneWindow);
        }

        Serialization::SerializeContentBrowserWindow(contentBrowserWindow);
        Serialization::SerializeConfig();
        ContentDiscovery::Shutdown();

        Singletons::Destruct();

        ImPlot::DestroyContext();

    }

    void App::Update(float deltaTime) {

        const ImGuiIO& io = ImGui::GetIO();

        // Will lead to frame time spikes, don't do it if content can't be seen
        if (!playingMaximized)
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

        auto lockMovement = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_LeftSuper);

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
            sceneWindow->Update(deltaTime);
        }

        ImGuizmo::Enable(activeSceneWindow->guizmo.needEnabled);

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

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        playingMaximized = false;
        auto activeSceneWindow = sceneWindows.empty() ? nullptr : sceneWindows[activeSceneIdx];
        if (activeSceneWindow) {
            playingMaximized = activeSceneWindow->isPlaying && activeSceneWindow->playMaximized;
        }
        if (!playingMaximized) {
            // The dockspace host is transparent, fill the gaps between the docked windows with the base color
            ImGui::GetBackgroundDrawList(viewport)->AddRectFilled(viewport->Pos,
                ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                ImGui::GetColorU32(Theme::Base()));

            // Status bar first, it reduces the work area of the viewport
            RenderStatusBar();

            // The host covers the work area, which excludes the main menu bar and the status bar
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::SetNextWindowBgAlpha(0.0f);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking |
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

            if (sceneWindows.empty())
                RenderEmptyState(mainDsId);

            if (ImGui::BeginMainMenuBar()) {
                static bool newScene = false;
                bool saveScene = false, exitEditor = false;
#ifdef AE_OS_MACOS
                const char* saveShortcut = "Cmd+S";
#else
                const char* saveShortcut = "Ctrl+S";
#endif
                if (ImGui::BeginMenu("File")) {
                    /*
                    ImGui::MenuItem("Open project", nullptr, &openProject);
                    ImGui::MenuItem("Save project", nullptr, &saveProject);
                    ImGui::Separator();
                    */
                    ImGui::MenuItem("New scene...", nullptr, &newScene);
                    RenderRecentScenesMenu();
                    ImGui::MenuItem("Save scene", saveShortcut, &saveScene, activeSceneWindow != nullptr);
                    ImGui::Separator();
                    ImGui::MenuItem("Exit", nullptr, &exitEditor);
                    /*
                    ImGui::Separator();
                    ImGui::MenuItem("Import files", nullptr, &importFiles);
                    */
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    ImGui::SeparatorText("Windows");
                    ImGui::MenuItem("Content browser", nullptr, &contentBrowserWindow.show);
                    ImGui::MenuItem("Log", nullptr, &logWindow.show);
                    ImGui::MenuItem("Profiler", nullptr, &profilerWindow.show);
                    ImGui::MenuItem("Geometry brush", nullptr, &geometryBrushWindow.show);

                    ImGui::SeparatorText("Appearance");
                    if (ImGui::MenuItem("Dark mode", nullptr, &config->darkMode)) {
                        Theme::Apply(config->darkMode);
                    }
                    ImGui::MenuItem("Reset layout", nullptr, &resetDockspaceLayout);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Renderer")) {
                    ImGui::MenuItem("VSync", nullptr, &config->vsync);
                    ImGui::MenuItem("Pathtracer", nullptr, &config->pathTrace);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Help")) {
                    if (ImGui::MenuItem("Command palette", "Ctrl+P"))
                        commandPalette.Open();
                    ImGui::MenuItem("Keyboard shortcuts", nullptr, &showShortcutsWindow);
                    ImGui::EndMenu();
                }

                if (newScene) {
                    UI::PopupPanels::isNewScenePopupVisible = true;
                    newScene = false;
                }

                if (saveScene && activeSceneWindow != nullptr) {
                    activeSceneWindow->SaveScene();
                }

                if (exitEditor)
                    Exit();

                ImGui::EndMainMenuBar();
            }            

            for (auto& sceneWindow : sceneWindows) {
                sceneWindow->Render();
            }

            // This does active changes to the scene, render after scene windows
            geometryBrushWindow.Render(activeSceneWindow);

            contentBrowserWindow.Render();
            logWindow.Render();
            profilerWindow.Render();

            ImGui::End();
        }
        else {
            RenderSceneMaximized();
        }

        // Global shortcut for the command palette
#ifdef AE_OS_MACOS
        bool commandDown = ImGui::GetIO().KeySuper;
#else
        bool commandDown = ImGui::GetIO().KeyCtrl;
#endif
        if (commandDown && ImGui::IsKeyPressed(ImGuiKey_P, false) && !Singletons::blockingOperation->block)
            commandPalette.Open();

        if (!playingMaximized)
            RenderShortcutsWindow();

        Notifications::Display();
        UI::PopupPanels::Render();
        commandPalette.Render();

        ImGui::Render();
        Singletons::imguiWrapper->Render(true);

    }

    void App::RenderSceneMaximized() {

        auto activeSceneWindow = sceneWindows.empty() ? nullptr : sceneWindows[activeSceneIdx];

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
        auto set = Singletons::imguiWrapper->GetTextureId(&activeSceneWindow->viewportPanel.viewportTexture);
        ImGui::Image(set, renderArea);

        if (activeSceneWindow->perfOverlayMaximized) {
            auto gpuProfilerData = Graphics::Profiler::GetQueriesAverage(32, Graphics::Profiler::OrderBy::MAX_TIME);

            std::string perfString;
            double slowestTime = 0.0;
            for (int32_t i = 0; i < int32_t(gpuProfilerData.size()); i++) {
                const auto& threadData = gpuProfilerData[i];
                double threadTime = 0.0;
                for (const auto& query : threadData.queries) {
                    threadTime += query.timer.elapsedTime;
                }
                if (threadTime > slowestTime) {
                    slowestTime = threadTime;
                }
            }

            auto target = Singletons::renderTarget;

            // Put the overlay on a translucent panel, such that it stays readable on bright scenes
            ImGui::SetCursorPos(ImVec2(12.0f, 12.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::OverlayBackground());
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::OverlayBorder());
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Theme::overlayRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
            ImGui::BeginChild("Performance overlay", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders |
                ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

            ImGui::SetWindowFontScale(1.25f);
            ImGui::Text("%.2f ms", Clock::GetAverage() * 1000.0f);
            ImGui::SameLine();
            ImGui::TextDisabled("frame");
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::Text("%.2f ms", float(slowestTime / 1000000.0));
            ImGui::SameLine();
            ImGui::TextDisabled("GPU");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("%dx%d upscaled from %dx%d", target->GetWidth(), target->GetHeight(),
                target->GetScaledWidth(), target->GetScaledHeight());

            performanceGraphPanel.Render(ivec2(400, 220), 0.0f);

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();

        ImGui::End();

    }

    void App::RenderEmptyState(ImGuiID dsID) {

        // The central node is transparent when nothing is docked, draw a hint behind all windows
        auto centralNode = ImGui::DockBuilderGetCentralNode(dsID);
        if (!centralNode || centralNode->Size.x <= 0.0f || centralNode->Size.y <= 0.0f)
            return;

        bool loading = !waitToLoadScenes.empty();
        const char* title = loading ? "Loading scene..." : "No scene open";
        const char* subtitle = loading ? "" :
            "Double-click a scene in the content browser or create one with File > New scene";

        auto drawList = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
        auto font = ImGui::GetFont();
        auto fontSize = ImGui::GetFontSize();
        auto titleSize = fontSize * 1.5f;

        auto center = ImVec2(centralNode->Pos.x + centralNode->Size.x * 0.5f,
            centralNode->Pos.y + centralNode->Size.y * 0.5f);

        auto titleWidth = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title).x;
        auto subtitleWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, subtitle).x;

        drawList->AddText(font, titleSize, ImVec2(center.x - titleWidth * 0.5f, center.y - titleSize),
            ImGui::GetColorU32(ImGuiCol_Text), title);
        drawList->AddText(font, fontSize, ImVec2(center.x - subtitleWidth * 0.5f, center.y + fontSize * 0.5f),
            ImGui::GetColorU32(Theme::TextMuted()), subtitle);

    }

    Ref<UI::SceneWindow> App::GetActiveSceneWindow() const {

        if (sceneWindows.empty())
            return nullptr;

        return sceneWindows[std::min(activeSceneIdx, sceneWindows.size() - 1)];

    }

    void App::RegisterCommands() {

        auto& config = Singletons::config;

        auto hasScene = [this]() {
            auto sceneWindow = GetActiveSceneWindow();
            return sceneWindow != nullptr && sceneWindow->scene.IsLoaded();
            };
        auto canEdit = [this, hasScene]() {
            return hasScene() && !GetActiveSceneWindow()->isPlaying;
            };

        // File
        commandPalette.Register({ .category = "File", .name = "New scene...",
            .action = []() { UI::PopupPanels::isNewScenePopupVisible = true; } });
        commandPalette.Register({ .category = "File", .name = "Save scene", .shortcut = "Ctrl+S",
            .action = [this]() { GetActiveSceneWindow()->SaveScene(); }, .isEnabled = canEdit });

        // Windows
        auto registerWindow = [&](const char* name, bool* show) {
            commandPalette.Register({ .category = "Window", .name = std::string("Toggle ") + name,
                .action = [show]() { *show = !*show; }, .isChecked = [show]() { return *show; } });
            };
        registerWindow("content browser", &contentBrowserWindow.show);
        registerWindow("log", &logWindow.show);
        registerWindow("profiler", &profilerWindow.show);
        registerWindow("geometry brush", &geometryBrushWindow.show);
        commandPalette.Register({ .category = "Window", .name = "Reset layout",
            .action = [this]() { resetDockspaceLayout = true; } });

        // Appearance and renderer
        commandPalette.Register({ .category = "View", .name = "Toggle dark mode",
            .action = [&config]() { config->darkMode = !config->darkMode; Theme::Apply(config->darkMode); },
            .isChecked = [&config]() { return config->darkMode; } });
        commandPalette.Register({ .category = "Renderer", .name = "Toggle VSync",
            .action = [&config]() { config->vsync = !config->vsync; },
            .isChecked = [&config]() { return config->vsync; } });
        commandPalette.Register({ .category = "Renderer", .name = "Toggle path tracer",
            .action = [&config]() { config->pathTrace = !config->pathTrace; },
            .isChecked = [&config]() { return config->pathTrace; } });

        // Viewport
        auto registerGizmoMode = [&](const char* name, const char* shortcut, int32_t mode) {
            commandPalette.Register({ .category = "Viewport", .name = name, .shortcut = shortcut,
                .action = [this, mode]() { GetActiveSceneWindow()->guizmo.mode = mode; },
                .isEnabled = canEdit,
                .isChecked = [this, mode]() { auto window = GetActiveSceneWindow(); return window && window->guizmo.mode == mode; } });
            };
        registerGizmoMode("Translate gizmo", "1", ImGuizmo::OPERATION::TRANSLATE);
        registerGizmoMode("Rotate gizmo", "2", ImGuizmo::OPERATION::ROTATE);
        registerGizmoMode("Scale gizmo", "3", ImGuizmo::OPERATION::SCALE);

        commandPalette.Register({ .category = "Viewport", .name = "Focus selection", .shortcut = "F",
            .action = [this]() { GetActiveSceneWindow()->FocusSelection(); }, .isEnabled = canEdit });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle grid", .shortcut = "G",
            .action = [this]() { auto window = GetActiveSceneWindow(); window->showGrid = !window->showGrid; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->showGrid; } });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle gizmo snapping",
            .action = [this]() { auto& guizmo = GetActiveSceneWindow()->guizmo; guizmo.snappingEnabled = !guizmo.snappingEnabled; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->guizmo.snappingEnabled; } });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle gizmo object space",
            .action = [this]() { auto& guizmo = GetActiveSceneWindow()->guizmo; guizmo.objectSpace = !guizmo.objectSpace; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->guizmo.objectSpace; } });

        auto registerVisualization = [&](const char* name, UI::ViewportVisualization visualization) {
            commandPalette.Register({ .category = "Visualization", .name = name,
                .action = [this, visualization]() { GetActiveSceneWindow()->viewportPanel.visualization = visualization; },
                .isEnabled = hasScene,
                .isChecked = [this, visualization]() {
                    auto window = GetActiveSceneWindow();
                    return window && window->viewportPanel.visualization == visualization; } });
            };
        registerVisualization("Lit", UI::ViewportVisualization::Lit);
        registerVisualization("Base color", UI::ViewportVisualization::GBufferBaseColor);
        registerVisualization("Roughness/Metalness/Ao", UI::ViewportVisualization::GBufferRoughnessMetalnessAo);
        registerVisualization("Normals", UI::ViewportVisualization::GBufferNormals);
        registerVisualization("Depth", UI::ViewportVisualization::GBufferDepth);
        registerVisualization("Velocity", UI::ViewportVisualization::GBufferVelocity);
        registerVisualization("Reflections", UI::ViewportVisualization::Reflections);
        registerVisualization("Clouds", UI::ViewportVisualization::Clouds);
        registerVisualization("SSGI", UI::ViewportVisualization::SSGI);

        // Create
        auto registerCreate = [&](const char* name, UI::SceneHierarchyPanel::EntityTemplate type) {
            commandPalette.Register({ .category = "Create", .name = name,
                .action = [this, type]() { GetActiveSceneWindow()->CreateEntityFromTemplate(type); },
                .isEnabled = canEdit });
            };
        registerCreate("Empty entity", UI::SceneHierarchyPanel::EntityTemplate::Empty);
        registerCreate("Point light", UI::SceneHierarchyPanel::EntityTemplate::PointLight);
        registerCreate("Spot light", UI::SceneHierarchyPanel::EntityTemplate::SpotLight);
        registerCreate("Directional light", UI::SceneHierarchyPanel::EntityTemplate::DirectionalLight);
        registerCreate("Camera", UI::SceneHierarchyPanel::EntityTemplate::Camera);
        registerCreate("Audio source", UI::SceneHierarchyPanel::EntityTemplate::AudioSource);
        registerCreate("Text", UI::SceneHierarchyPanel::EntityTemplate::Text);

        // Scene
        commandPalette.Register({ .category = "Scene", .name = "Play",
            .action = [this]() { GetActiveSceneWindow()->StartPlaying(); }, .isEnabled = canEdit });
        commandPalette.Register({ .category = "Scene", .name = "Stop playing", .shortcut = "Ctrl+Q",
            .action = [this]() { GetActiveSceneWindow()->StopPlaying(); },
            .isEnabled = [this, hasScene]() { return hasScene() && GetActiveSceneWindow()->isPlaying; } });

        // Help
        commandPalette.Register({ .category = "Help", .name = "Keyboard shortcuts",
            .action = [this]() { showShortcutsWindow = true; } });

    }

    void App::RenderRecentScenesMenu() {

        auto& config = Singletons::config;

        if (!ImGui::BeginMenu("Open recent", !config->recentScenes.empty()))
            return;

        std::string openPath;
        for (const auto& path : config->recentScenes) {
            // Scenes might have been moved or deleted in the meantime
            bool exists = std::filesystem::exists(Loader::AssetLoader::GetFullPath(path));
            // An opened scene can still be loading or might have failed to load
            auto openedIter = std::find_if(config->openedScenes.begin(), config->openedScenes.end(),
                [&](const ResourceHandle<Scene::Scene>& handle) { return handle.IsValid() && handle.GetResource()->path == path; });
            bool isOpened = openedIter != config->openedScenes.end();
            bool loadFailed = isOpened && openedIter->GetResource()->errorOnLoad;
            const char* state = !isOpened ? nullptr : openedIter->IsLoaded() ? "open" : loadFailed ? "failed" : "loading";

            auto name = Common::Path::GetFileNameWithoutExtension(path);
            if (ImGui::MenuItem(name.c_str(), state, false, exists && !isOpened))
                openPath = path;

            if (!exists)
                ImGui::SetItemTooltip("File doesn't exist anymore");
            else if (loadFailed)
                ImGui::SetItemTooltip("Loading failed: %s", openedIter->GetResource()->exceptionOnLoad.what());
            else
                ImGui::SetItemTooltip("%s", path.c_str());
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Clear list"))
            config->recentScenes.clear();

        ImGui::EndMenu();

        if (!openPath.empty())
            FileImporter::ImportFile<Scene::Scene>(openPath);

    }

    void App::RenderStatusBar() {

        auto viewport = ImGui::GetMainViewport();
        auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        auto divider = []() {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            };

        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Theme::Base());
        if (ImGui::BeginViewportSideBar("##Status bar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight(), flags)) {
            if (ImGui::BeginMenuBar()) {
                auto sceneWindow = GetActiveSceneWindow();
                if (sceneWindow && sceneWindow->scene.IsLoaded()) {
                    if (sceneWindow->isPlaying)
                        ImGui::TextColored(Theme::Success(), "Playing");
                    else
                        ImGui::TextDisabled("Editing");

                    divider();
                    ImGui::TextUnformatted(sceneWindow->scene->name.c_str());

                    divider();
                    const auto& selection = sceneWindow->sceneHierarchyPanel.selectedEntities;
                    auto selectedEntity = sceneWindow->sceneHierarchyPanel.selectedEntity;
                    if (selection.empty()) {
                        ImGui::TextDisabled("Nothing selected");
                    }
                    else if (selection.size() == 1 && selectedEntity.IsValid()) {
                        auto nameComponent = selectedEntity.TryGetComponent<NameComponent>();
                        auto name = nameComponent ? nameComponent->name : "Entity " + std::to_string(selectedEntity);
                        ImGui::TextDisabled("Selected");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(name.c_str());
                    }
                    else {
                        ImGui::Text("%d entities selected", int32_t(selection.size()));
                    }

                    divider();
                    const auto& guizmo = sceneWindow->guizmo;
                    const char* modeName = guizmo.mode == ImGuizmo::OPERATION::ROTATE ? "Rotate" :
                        guizmo.mode == ImGuizmo::OPERATION::SCALE ? "Scale" : "Translate";
                    ImGui::TextDisabled("%s, %s%s", modeName, guizmo.objectSpace ? "local" : "world",
                        guizmo.snappingEnabled ? ", snapping" : "");
                }
                else {
                    ImGui::TextDisabled("No scene open");
                }

                // Performance on the right side
                auto frameTime = Clock::GetAverage() * 1000.0f;
                char rightText[64];
                std::snprintf(rightText, sizeof(rightText), "%.1f ms  %.0f fps",
                    frameTime, frameTime > 0.0f ? 1000.0f / frameTime : 0.0f);
                auto rightWidth = ImGui::CalcTextSize(rightText).x + ImGui::GetStyle().ItemSpacing.x;
                auto rightPos = ImGui::GetWindowContentRegionMax().x - rightWidth;
                if (rightPos > ImGui::GetCursorPosX()) {
                    ImGui::SameLine(rightPos);
                    ImGui::TextDisabled("%s", rightText);
                }

                ImGui::EndMenuBar();
            }
        }
        // Side bars always need to be ended
        ImGui::End();
        ImGui::PopStyleColor();

    }

    void App::RenderShortcutsWindow() {

        if (!showShortcutsWindow)
            return;

        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("Keyboard shortcuts", &showShortcutsWindow, flags)) {
            auto section = [](const char* title, std::initializer_list<std::pair<const char*, const char*>> rows) {
                ImGui::SeparatorText(title);
                if (ImGui::BeginTable(title, 2, ImGuiTableFlags_SizingFixedFit)) {
                    // Same key column width in every section, such that the descriptions line up
                    ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Left mouse drag").x + ImGui::GetFontSize());
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed);
                    for (const auto& [keys, description] : rows) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(keys);
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", description);
                    }
                    ImGui::EndTable();
                }
                };

            section("General", {
                { "Ctrl+P", "Open the command palette" },
                { "Ctrl+S", "Save the active scene" },
                { "Ctrl+Q", "Stop playing" },
                });
            section("Viewport", {
                { "Left mouse drag", "Rotate the camera" },
                { "W / A / S / D", "Move the camera" },
                { "Q / E", "Move the camera down / up" },
                { "Hold Ctrl", "Lock camera movement" },
                { "Right click", "Select entity (Shift to add)" },
                { "1 / 2 / 3", "Translate / rotate / scale gizmo" },
                { "F", "Focus the selection" },
                { "G", "Toggle the ground grid" },
                { "Mouse wheel", "Change the camera speed" },
                });
            section("Scene hierarchy", {
                { "Double click", "Focus the entity" },
                { "F2", "Rename the selected entity" },
                { "Shift+Click", "Add to the selection" },
                { "Ctrl+D", "Duplicate the selection" },
                { "Delete", "Delete the selection" },
                { "Drag and drop", "Reparent, or create a prefab in the content browser" },
                });
            section("Content browser", {
                { "Double click", "Open a folder or scene" },
                { "Ctrl + mouse wheel", "Change the icon size" },
                });
            section("Terrain editing", {
                { "Hold right mouse", "Paint with the active brush" },
                });
        }
        ImGui::End();

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
                Singletons::config->AddRecentScene(scene->path);
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
