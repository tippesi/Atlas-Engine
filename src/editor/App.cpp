#include "App.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Serialization.h"
#include "Notifications.h"
#include "ContentDiscovery.h"
#include "Theme.h"
#include "DataCreator.h"
#include "ui/panels/PopupPanels.h"
#include "tools/FileSystemHelper.h"
#include <ImGuizmo.h>
#include <profiler/CPUProfiler.h>
#include <profiler/HitchRecorder.h>
#include <streaming/StreamingManager.h>
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

        // Scenes don't wait behind the meshes and textures which are already loading
        ResourceManager<Scene::Scene>::loadPriority = JobPriority::Medium;

        // Meshes and impostors are saved and baked here, they stay resident unless the engine settings enable it
        Streaming::StreamingManager::settings.impostors.enable = false;
        Streaming::StreamingManager::settings.meshes.enable = false;

        // --log-hitches [ms] logs frames that take much longer than the ones before with their CPU profile
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] != "--log-hitches")
                continue;
            HitchRecorder::log = true;
            if (i + 1 < args.size() && std::isdigit(static_cast<unsigned char>(args[i + 1][0])))
                HitchRecorder::minimumFrameTime = std::stof(args[i + 1]);
        }

        ContentDiscovery::Update();

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        SetDefaultWindowResolution();

        auto icon = Atlas::Texture::Texture2D("icon.png");
        window.SetIcon(&icon);

        SubscribeToResourceEvents();

        Singletons::imguiWrapper = CreateRef<ImguiExtension::ImguiWrapper>();
        Singletons::config = CreateRef<Config>();

        Serialization::DeserializeConfig();
        Serialization::DeserializeContentBrowserWindow(contentBrowserWindow);

        Serialization::DeserializeLogWindow(logWindow);
        restoringScenes = false;

        // The font depends on the scale from the config, its resources are created only once
        UpdateUiScale();
        Singletons::imguiWrapper->Load(&window);

        // Everything that needs the config comes afterwards
        Singletons::icons = CreateRef<Icons>();
        Singletons::blockingOperation = CreateRef<BlockingOperation>();
        Singletons::mainRenderer = mainRenderer;

        mouseHandler = Input::MouseHandler(1.5f, 8.0f);
        keyboardHandler = Input::KeyboardHandler(7.0f, 5.0f);

        ApplyTheme();

        RegisterCommands();

    }

    void App::UnloadContent() {

        Loader::ModelImporter::WaitForBackgroundImports();

        Singletons::imguiWrapper->Unload();

        CopyPasteHelper::Clear();

        for (const auto& sceneWindow : sceneWindows) {
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            if (!sceneWindow->IsPrefab())
                Serialization::SerializeSceneWindow(sceneWindow);
        }

        Serialization::SerializeContentBrowserWindow(contentBrowserWindow);
        Serialization::SerializeLogWindow(logWindow);
        Serialization::SerializeConfig();
        ContentDiscovery::Shutdown();

        Singletons::Destruct();

        ImPlot::DestroyContext();

    }

    void App::Update(float deltaTime) {

        const ImGuiIO& io = ImGui::GetIO();

        CPUProfiler::Scope sectionScope("Content discovery");
        // Will lead to frame time spikes, don't do it if content can't be seen
        if (!playingMaximized)
            ContentDiscovery::Update();
        sectionScope.Next("ImGui update");
        UpdateUiScale();
        Singletons::imguiWrapper->Update(&window, deltaTime);

        Singletons::blockingOperation->Update();

        if (Singletons::blockingOperation->block)
            return;

        Loader::ModelImporter::ApplyBackgroundImports();

        sectionScope.Next("Content browser update");
        contentBrowserWindow.Update();
        logWindow.Update();

        // Requests to show a file also have to work while the drawer is closed
        if (!UI::ContentBrowserWindow::contentToShowPath.empty())
            contentBrowserWindow.show = true;

        if (!UI::ContentBrowserWindow::contentToEditPath.empty()) {
            OpenInAssetEditor(UI::ContentBrowserWindow::contentToEditPath);
            UI::ContentBrowserWindow::contentToEditPath.clear();
        }

        std::erase_if(assetEditorWindows, [](const auto& window) { return !window->show; });
        if (activeAssetEditorWindow && !activeAssetEditorWindow->show)
            activeAssetEditorWindow = nullptr;
        for (const auto& assetEditorWindow : assetEditorWindows)
            if (assetEditorWindow->inFocus)
                activeAssetEditorWindow = assetEditorWindow;
        // Drags outside of viewports, e.g. on a slider, must not turn the next camera that gets the focus
        auto mouseMovedCamera = false;
        for (const auto& assetEditorWindow : assetEditorWindows) {
            if (!assetEditorWindow->viewportPanel.isFocused || !assetEditorWindow->cameraEntity.IsValid())
                continue;
            auto& camera = assetEditorWindow->cameraEntity.GetComponent<CameraComponent>();
            keyboardHandler.speed = assetEditorWindow->cameraMovementSpeed;
            mouseHandler.Update(camera, deltaTime);
            keyboardHandler.Update(camera, deltaTime);
            mouseMovedCamera = true;
        }

        sectionScope.Next("Scene windows update");

        // Create new windows for fully loaded scenes
        for (size_t i = 0; i < waitToLoadScenes.size(); i++) {
            if (waitToLoadScenes[i].IsLoaded()) {
                std::swap(waitToLoadScenes[i], waitToLoadScenes.back());

                // After the scene is loaded we can retrieve the window configuration and deserialize it
                sceneWindows.emplace_back(Serialization::DeserializeSceneWindow(waitToLoadScenes.back()));
                // Scenes opened by the user and the ones opened for the first time join the others, restored
                // ones stay where they were left and after a layout reset nobody has a place anymore
                auto settings = ImGui::FindWindowSettingsByID(ImHashStr(sceneWindows.back()->GetNameID()));
                sceneWindows.back()->requestDocking = !settings || dockLoadingScenes;
                // Large scenes take a while, it's worth to say when they are there
                if (std::erase(userOpenedScenes, waitToLoadScenes.back())) {
                    OpenAsTab(*sceneWindows.back());
                    Notifications::Push({ .message = "Opened scene " + waitToLoadScenes.back()->name });
                }
                waitToLoadScenes.pop_back();
                i--;
            }
        }

        // Failed scenes are forgotten, such that they can be opened again once the problem is fixed
        std::erase_if(waitToLoadScenes, [&](const auto& handle) {
            if (!handle.GetResource()->errorOnLoad)
                return false;
            std::erase(Singletons::config->openedScenes, handle);
            std::erase(userOpenedScenes, handle);
            Notifications::Push({ .message = "Couldn't load scene " + handle.GetResource()->GetFileName() + ": " +
                handle.GetResource()->errorMessageOnLoad, .type = NotificationType::Error });
            return true;
            });

        if (waitToLoadScenes.empty())
            dockLoadingScenes = false;

        // Quick quit
        if (sceneWindows.empty()) {
            if (!mouseMovedCamera)
                mouseHandler.Reset();
            return;
        }

        auto& config = Singletons::config;

        std::vector<size_t> toBeDeletedSceneWindows;

        // Find active scene and old scene windows
        size_t sceneCounter = 0;
        for (const auto& sceneWindow : sceneWindows) {

            // Delete windows not opened in the config (don't know when this does happen), prefabs aren't part of it
            if (!sceneWindow->IsPrefab() && std::find(config->openedScenes.begin(), config->openedScenes.end(),
                sceneWindow->scene) == config->openedScenes.end()) {
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            // Delete closed windows
            if (!sceneWindow->show) {
                auto iter = std::find(config->openedScenes.begin(), config->openedScenes.end(),
                    sceneWindow->scene);
                if (iter != config->openedScenes.end())
                    config->openedScenes.erase(iter);
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            if (sceneWindow->inFocus) {
                activeSceneIdx = sceneCounter;
                activeAssetEditorWindow = nullptr;
            }

            sceneCounter++;
        }

        // Reverse such that indices remain valid
        std::reverse(toBeDeletedSceneWindows.begin(), toBeDeletedSceneWindows.end());
        for (auto sceneWindowIdx : toBeDeletedSceneWindows) {
            // Serialize window before erasing it
            if (sceneWindows[sceneWindowIdx]->isPlaying)
                sceneWindows[sceneWindowIdx]->StopPlaying();
            if (!sceneWindows[sceneWindowIdx]->IsPrefab())
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
            mouseMovedCamera = true;
        }

        if (!mouseMovedCamera)
            mouseHandler.Reset();

        if (activeSceneWindow->isPlaying && !activeSceneWindow->scene->paused) {
            playerInput.mouseHandler.sensibility = activeSceneWindow->cameraRotationSpeed;
            playerInput.Update(*activeSceneWindow->scene.Get(), deltaTime);
        }

        // Update all scenes after input was applied
        for (auto& sceneWindow : sceneWindows) {
            // Only the last focused scene uses the camera input and the gizmo
            if (sceneWindow == activeSceneWindow && !activeAssetEditorWindow)
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

        auto waitForSceneUpdates = [&]() {

            for (const auto& sceneWindow : sceneWindows) {
                if (sceneWindow->scene.IsLoaded())
                    sceneWindow->scene->WaitForAsyncWorkCompletion();
            }

            // The preview scenes of asset editors are updated even if their viewport isn't rendered
            for (const auto& assetEditorWindow : assetEditorWindows) {
                if (assetEditorWindow->scene)
                    assetEditorWindow->scene->WaitForAsyncWorkCompletion();
            }

        };

        if (window.IsOccluded()) {
            waitForSceneUpdates();
            return;
        }


        auto& config = Singletons::config;

        auto swapChainVsync = graphicsDevice->swapChain->presentMode == VK_PRESENT_MODE_FIFO_KHR;
        if (swapChainVsync != config->vsync) {
            if (config->vsync) graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_FIFO_KHR, Atlas::Graphics::SRGB_NONLINEAR);
            else graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_IMMEDIATE_KHR, Atlas::Graphics::SRGB_NONLINEAR);
        }






        CPUProfiler::Scope sectionScope("ImGui new frame");
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
            sectionScope.Next("Status bar");
            RenderStatusBar();
            sectionScope.Next("Dockspace");

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
            PrepareDrawerDocking(mainDsId);

            // The tabs have close buttons, a second one at the end of the tab bar would just be confusing
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton;
            ImGui::DockSpace(mainDsId, ImVec2(0.0f, 0.0f), dockspace_flags);

            // Layouts saved before the drawers existed have no central node to dock into
            if (!ImGui::DockBuilderGetCentralNode(mainDsId))
                resetDockspaceLayout = true;

            if (sceneWindows.empty())
                RenderEmptyState(mainDsId);
            else
                emptyStateVisible = false;

            sectionScope.Next("Main menu bar");
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

                if (ImGui::BeginMenu("Edit")) {
                    auto history = GetActiveHistory();
                    auto undoName = history ? history->GetUndoName() : std::string();
                    auto redoName = history ? history->GetRedoName() : std::string();
                    if (ImGui::MenuItem(undoName.empty() ? "Undo" : ("Undo " + undoName).c_str(), "Ctrl+Z", false, CanUndo()))
                        Undo();
                    if (ImGui::MenuItem(redoName.empty() ? "Redo" : ("Redo " + redoName).c_str(), "Ctrl+Shift+Z", false, CanRedo()))
                        Redo();
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    ImGui::SeparatorText("Windows");
                    ImGui::MenuItem("Content browser", "Ctrl+B", &contentBrowserWindow.show);
                    ImGui::MenuItem("Log", "Ctrl+J", &logWindow.show);
                    ImGui::MenuItem("Profiler", nullptr, &profilerWindow.show);
                    ImGui::MenuItem("Engine settings", nullptr, &settingsWindow.show);
                    ImGui::MenuItem("Geometry brush", nullptr, &geometryBrushWindow.show);

                    ImGui::SeparatorText("Appearance");
                    if (ImGui::MenuItem("Dark mode", nullptr, &config->darkMode)) {
                        ApplyTheme();
                    }
                    if (ImGui::BeginMenu("Interface scale")) {
                        auto percent = [](float scale) { return std::to_string(int32_t(std::round(scale * 100.0f))) + "%"; };
                        auto systemLabel = "System (" + percent(window.GetContentScale()) + ")";
                        if (ImGui::MenuItem(systemLabel.c_str(), nullptr, config->uiScale <= 0.0f))
                            config->uiScale = 0.0f;
                        ImGui::Separator();
                        for (auto scale : { 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f }) {
                            if (ImGui::MenuItem(percent(scale).c_str(), nullptr, config->uiScale == scale))
                                config->uiScale = scale;
                        }
                        ImGui::EndMenu();
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

            sectionScope.Next("Scene windows");
            for (auto& sceneWindow : sceneWindows) {
                DockNextToScenes(*sceneWindow);
                sceneWindow->Render();
            }

            sectionScope.Next("Asset editor windows");
            for (auto& assetEditorWindow : assetEditorWindows) {
                DockNextToScenes(*assetEditorWindow);
                assetEditorWindow->Render();
            }

            // This does active changes to the scene, render after scene windows
            sectionScope.Next("Geometry brush window");
            geometryBrushWindow.Render(activeSceneWindow);

            UpdateDrawers();

            sectionScope.Next("Content browser");
            DockPinnedDrawer(contentBrowserWindow);
            contentBrowserWindow.Render();
            sectionScope.Next("Log window");
            DockPinnedDrawer(logWindow);
            logWindow.Render();
            sectionScope.Next("Profiler window");
            profilerWindow.Render();
            sectionScope.Next("Settings window");
            settingsWindow.Render();

            ImGui::End();
        }
        else {
            sectionScope.Next("Maximized scene");
            RenderSceneMaximized();
        }
        sectionScope.Next("Popups and notifications");

        // Global shortcut for the command palette
#ifdef AE_OS_MACOS
        bool commandDown = ImGui::GetIO().KeySuper;
#else
        bool commandDown = ImGui::GetIO().KeyCtrl;
#endif
        if (commandDown && ImGui::IsKeyPressed(ImGuiKey_P, false) && !Singletons::blockingOperation->block)
            commandPalette.Open();
        if (commandDown && ImGui::IsKeyPressed(ImGuiKey_B, false) && !playingMaximized)
            contentBrowserWindow.show = !contentBrowserWindow.show;
        if (commandDown && ImGui::IsKeyPressed(ImGuiKey_J, false) && !playingMaximized)
            logWindow.show = !logWindow.show;

        if (!playingMaximized)
            RenderShortcutsWindow();

        Notifications::Display();
        UI::PopupPanels::Render();
        commandPalette.Render();

        // Inactive viewports can still have scene jobs that enqueue GPU work
        sectionScope.Next("Wait for scene updates");
        waitForSceneUpdates();
        sectionScope.Next("ImGui render");
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
            ivec2(0), ivec2(int32_t(renderArea.x), int32_t(renderArea.y)));
        auto set = Singletons::imguiWrapper->GetTextureId(&activeSceneWindow->viewportPanel.viewportTexture);
        ImGui::Image(set, renderArea);

        if (activeSceneWindow->perfOverlayMaximized) {
            auto gpuProfilerData = GPUProfiler::GetQueriesAverage(32, GPUProfiler::OrderBy::MAX_TIME);

            std::string perfString;
            double slowestTime = 0.0;
            for (int32_t i = 0; i < int32_t(gpuProfilerData.size()); i++) {
                const auto& threadData = gpuProfilerData[i];
                double threadTime = 0.0;
                for (const auto& query : threadData.queries) {
                    threadTime += query.elapsedTime;
                }
                if (threadTime > slowestTime) {
                    slowestTime = threadTime;
                }
            }

            auto target = activeSceneWindow->viewportPanel.renderTarget;

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

        auto centralNode = ImGui::DockBuilderGetCentralNode(dsID);
        if (!centralNode || centralNode->Size.x <= 0.0f || centralNode->Size.y <= 0.0f)
            return;

        auto& config = Singletons::config;

        ImGui::SetNextWindowPos(ImVec2(centralNode->Pos.x + centralNode->Size.x * 0.5f,
            centralNode->Pos.y + centralNode->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        // Stays behind everything else, e.g. the drawers
        auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("##Empty state", nullptr, flags);

        if (!waitToLoadScenes.empty()) {
            // The spinner is centered above the heading, the window sizes itself to the heading
            auto textWidth = ImGui::CalcTextSize("Loading scene...").x * UI::Widgets::headingScale;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (textWidth - ImGui::GetTextLineHeight() * 2.0f) * 0.5f));
            UI::Widgets::Spinner(2.0f);
            UI::Widgets::Heading("Loading scene...");
            ImGui::End();
            return;
        }

        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextUnformatted("No scene open");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("Create a new scene or open one from the content browser");

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        Theme::PushPrimaryButton();
        if (ImGui::Button("New scene..."))
            UI::PopupPanels::isNewScenePopupVisible = true;
        Theme::PopPrimaryButton();
        ImGui::SameLine();
        if (ImGui::Button("Content browser"))
            contentBrowserWindow.show = true;

        // Scenes might have been moved or deleted in the meantime, which is checked once when the state shows up
        if (!emptyStateVisible) {
            emptyStateScenes.clear();
            for (const auto& path : config->recentScenes) {
                if (emptyStateScenes.size() < 5 && std::filesystem::exists(Loader::AssetLoader::GetFullPath(path)))
                    emptyStateScenes.push_back(path);
            }
        }
        emptyStateVisible = true;

        if (!emptyStateScenes.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::SeparatorText("Recent scenes");
        }

        std::string openPath;
        for (const auto& path : emptyStateScenes) {
            auto name = Common::Path::GetFileNameWithoutExtension(path);
            if (ImGui::Selectable((name + "##" + path).c_str()))
                openPath = path;
            ImGui::SetItemTooltip("%s", path.c_str());
        }

        ImGui::End();

        if (!openPath.empty())
            FileImporter::ImportFile<Scene::Scene>(openPath);

    }

    void App::ApplyTheme() {

        Theme::Apply(Singletons::config->darkMode);

        auto base = Theme::Base();
        window.SetTitleBarColor(vec3(base.x, base.y, base.z), Singletons::config->darkMode);

    }

    void App::UpdateUiScale() {

        auto systemScale = window.GetContentScale();
        auto scale = glm::clamp(Singletons::config->uiScale > 0.0f ? Singletons::config->uiScale : systemScale, 0.5f, 3.0f);

        // The window was moved to a display with another scale
        if (Singletons::config->uiScale <= 0.0f && systemUiScale > 0.0f && systemScale != systemUiScale)
            Notifications::Push({ .message = "Scaled the interface to " + std::to_string(int32_t(std::round(scale * 100.0f))) +
                "% for this display" });
        systemUiScale = systemScale;

        Singletons::imguiWrapper->SetScale(scale);

        // Minimized windows can have no size at all, the font is kept for when they come back
        auto drawableSize = window.GetDrawableSize();
        if (drawableSize.x <= 0 || window.GetWidth() <= 0)
            return;

        // Text is rasterized for the pixels it covers, but at least at twice its size such that larger text stays sharp
        auto pixelDensity = float(drawableSize.x) / float(window.GetWidth());
        auto rasterSize = std::round(16.0f * std::max(2.0f, pixelDensity * scale));
        if (rasterSize == fontRasterSize)
            return;
        fontRasterSize = rasterSize;

        auto& io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(), rasterSize)->Scale = 16.0f / rasterSize;
        Singletons::imguiWrapper->RecreateImGuiResources();

    }

    void App::Undo() {

        if (activeAssetEditorWindow) {
            activeAssetEditorWindow->Undo();
            return;
        }

        auto sceneWindow = GetActiveSceneWindow();
        if (sceneWindow && !sceneWindow->isPlaying)
            sceneWindow->Undo();

    }

    void App::Redo() {

        if (activeAssetEditorWindow) {
            activeAssetEditorWindow->Redo();
            return;
        }

        auto sceneWindow = GetActiveSceneWindow();
        if (sceneWindow && !sceneWindow->isPlaying)
            sceneWindow->Redo();

    }

    bool App::CanUndo() const {

        auto history = GetActiveHistory();
        return history && history->CanUndo();

    }

    bool App::CanRedo() const {

        auto history = GetActiveHistory();
        return history && history->CanRedo();

    }

    const UndoHistory* App::GetActiveHistory() const {

        if (activeAssetEditorWindow)
            return &activeAssetEditorWindow->history;

        auto sceneWindow = GetActiveSceneWindow();
        if (!sceneWindow || sceneWindow->isPlaying)
            return nullptr;
        return &sceneWindow->history.history;

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
        auto registerWindow = [&](const char* name, bool* show, const char* shortcut = "") {
            commandPalette.Register({ .category = "Window", .name = std::string("Toggle ") + name, .shortcut = shortcut,
                .action = [show]() { *show = !*show; }, .isChecked = [show]() { return *show; } });
            };
        registerWindow("content browser", &contentBrowserWindow.show, "Ctrl+B");
        registerWindow("log", &logWindow.show, "Ctrl+J");
        auto registerPin = [&](const char* name, UI::DrawerWindow* drawer) {
            commandPalette.Register({ .category = "Window", .name = std::string("Dock ") + name + " in layout",
                .action = [drawer]() { drawer->SetPinned(!drawer->pinned); },
                .isChecked = [drawer]() { return drawer->pinned; } });
            };
        registerPin("content browser", &contentBrowserWindow);
        registerPin("log", &logWindow);
        registerWindow("profiler", &profilerWindow.show);
        registerWindow("engine settings", &settingsWindow.show);
        registerWindow("geometry brush", &geometryBrushWindow.show);
        commandPalette.Register({ .category = "Window", .name = "Reset layout",
            .action = [this]() { resetDockspaceLayout = true; } });

        // Appearance and renderer
        commandPalette.Register({ .category = "View", .name = "Toggle dark mode",
            .action = [this, &config]() { config->darkMode = !config->darkMode; ApplyTheme(); },
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
        commandPalette.Register({ .category = "Viewport", .name = "Drop selection to ground", .shortcut = "End",
            .action = [this]() { GetActiveSceneWindow()->DropSelectionToGround(); }, .isEnabled = canEdit });
        auto isViewingCamera = [this, canEdit]() {
            return canEdit() && GetActiveSceneWindow()->viewedCameraEntity.IsValid();
            };
        commandPalette.Register({ .category = "Viewport", .name = "View through selected camera",
            .action = [this]() { auto window = GetActiveSceneWindow(); window->ViewThroughCamera(window->sceneHierarchyPanel.selectedEntity); },
            .isEnabled = [this, canEdit]() {
                if (!canEdit())
                    return false;
                auto entity = GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntity;
                return entity.IsValid() && entity.HasComponent<CameraComponent>();
                } });
        commandPalette.Register({ .category = "Viewport", .name = "Apply view to camera",
            .action = [this]() { GetActiveSceneWindow()->ApplyViewToCamera(); }, .isEnabled = isViewingCamera });
        commandPalette.Register({ .category = "Viewport", .name = "Back to editor camera",
            .action = [this]() { GetActiveSceneWindow()->ReturnToEditorCamera(); }, .isEnabled = isViewingCamera });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle grid", .shortcut = "G",
            .action = [this]() { auto window = GetActiveSceneWindow(); window->showGrid = !window->showGrid; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->showGrid; } });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle physics",
            .action = [this]() { auto window = GetActiveSceneWindow(); window->showPhysics = !window->showPhysics; },
            .isEnabled = hasScene,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->showPhysics; } });
        commandPalette.Register({ .category = "Viewport", .name = "Toggle navigation mesh",
            .action = [this]() { auto& panel = GetActiveSceneWindow()->scenePropertiesPanel.navigationPanel; panel.showMesh = !panel.showMesh; },
            .isEnabled = hasScene,
            .isChecked = [this]() { auto window = GetActiveSceneWindow(); return window && window->scenePropertiesPanel.navigationPanel.showMesh; } });
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
        registerCreate("Fog volume", UI::SceneHierarchyPanel::EntityTemplate::FogVolume);

        // Scene
        commandPalette.Register({ .category = "Scene", .name = "Play",
            .action = [this]() { GetActiveSceneWindow()->StartPlaying(); }, .isEnabled = canEdit });
        commandPalette.Register({ .category = "Scene", .name = "Pause or resume playing",
            .action = [this]() { GetActiveSceneWindow()->TogglePause(); },
            .isEnabled = [this, hasScene]() { return hasScene() && GetActiveSceneWindow()->isPlaying; } });
        commandPalette.Register({ .category = "Scene", .name = "Stop playing", .shortcut = "Ctrl+Q",
            .action = [this]() { GetActiveSceneWindow()->StopPlaying(); },
            .isEnabled = [this, hasScene]() { return hasScene() && GetActiveSceneWindow()->isPlaying; } });

        // Undo of the asset editor or scene in front
        commandPalette.Register({ .category = "Edit", .name = "Undo", .shortcut = "Ctrl+Z",
            .action = [this]() { Undo(); },
            .isEnabled = [this]() { return CanUndo(); } });
        commandPalette.Register({ .category = "Edit", .name = "Redo", .shortcut = "Ctrl+Shift+Z",
            .action = [this]() { Redo(); },
            .isEnabled = [this]() { return CanRedo(); } });

        // Material graph of the material editor in front
        auto materialEditor = [this]() {
            return std::dynamic_pointer_cast<UI::MaterialEditorWindow>(activeAssetEditorWindow);
            };
        auto hasMaterialGraph = [materialEditor]() {
            auto window = materialEditor();
            return window && window->material.IsLoaded() && window->material->HasGraph();
            };
        commandPalette.Register({ .category = "Material graph", .name = "Add node", .shortcut = "Space",
            .action = [materialEditor]() { materialEditor()->graphPanel.OpenAddMenu(); }, .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Frame all nodes", .shortcut = "F",
            .action = [materialEditor]() { materialEditor()->graphPanel.FrameAll(); }, .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Arrange nodes",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.Arrange(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Show generated code",
            .action = [materialEditor]() { materialEditor()->graphPanel.ShowGeneratedCode(); }, .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Select all nodes", .shortcut = "Ctrl+A",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.SelectAll(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Delete selected nodes", .shortcut = "Del",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.DeleteSelection(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Copy selected nodes", .shortcut = "Ctrl+C",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.CopySelection(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Paste nodes", .shortcut = "Ctrl+V",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.Paste(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Duplicate selected nodes", .shortcut = "Ctrl+D",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.DuplicateSelection(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Frame selected nodes", .shortcut = "Ctrl+G",
            .action = [materialEditor]() { auto window = materialEditor(); window->graphPanel.FrameSelection(*window->material->graph.Get()); },
            .isEnabled = hasMaterialGraph });
        commandPalette.Register({ .category = "Material graph", .name = "Toggle maximized", .shortcut = "F11",
            .action = [materialEditor]() { materialEditor()->graphPanel.ToggleMaximized(); }, .isEnabled = hasMaterialGraph });

        // Asset editor in front
        auto hasAssetEditor = [this]() { return activeAssetEditorWindow != nullptr; };
        commandPalette.Register({ .category = "Asset editor", .name = "Save asset", .shortcut = "Ctrl+S",
            .action = [this]() { activeAssetEditorWindow->Save(); }, .isEnabled = hasAssetEditor });
        commandPalette.Register({ .category = "Asset editor", .name = "Focus asset", .shortcut = "F",
            .action = [this]() { activeAssetEditorWindow->FocusAsset(); }, .isEnabled = hasAssetEditor });
        commandPalette.Register({ .category = "Asset editor", .name = "Toggle grid", .shortcut = "G",
            .action = [this]() { activeAssetEditorWindow->showGrid = !activeAssetEditorWindow->showGrid; }, .isEnabled = hasAssetEditor });

        // Animation timeline of the mesh editor in front
        auto meshEditor = [this]() {
            return std::dynamic_pointer_cast<UI::MeshEditorWindow>(activeAssetEditorWindow);
            };
        commandPalette.Register({ .category = "Animation timeline", .name = "Save animation",
            .action = [meshEditor]() { meshEditor()->timelinePanel.saveRequested = true; },
            .isEnabled = [meshEditor]() { return meshEditor() != nullptr; } });
        commandPalette.Register({ .category = "Animation timeline", .name = "Delete selected key", .shortcut = "Del",
            .action = [meshEditor]() { meshEditor()->timelinePanel.deleteKeyRequested = true; },
            .isEnabled = [meshEditor]() { auto window = meshEditor(); return window && window->timelinePanel.HasSelectedKey(); } });

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
                ImGui::SetItemTooltip("Loading failed: %s", openedIter->GetResource()->errorMessageOnLoad.c_str());
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

        auto gap = [](float width) {
            ImGui::SameLine(0.0f, width);
            };

        // Plain text shares the vertical center with the labels of the chips
        auto text = [&](const char* str, ImGuiCol color) {
            auto pos = ImGui::GetCursorScreenPos();
            auto height = ImGui::GetFrameHeight();
            ImGui::Dummy(ImVec2(ImGui::CalcTextSize(str).x, height));
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, GetCapCenteredTextY(pos.y + height * 0.5f)),
                ImGui::GetColorU32(color), str);
            };

        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Theme::Base());
        if (ImGui::BeginViewportSideBar("##Status bar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight() + 4.0f, flags)) {
            if (ImGui::BeginMenuBar()) {
                auto window = ImGui::GetCurrentWindow();
                window->DrawList->AddLine(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y),
                    ImGui::GetColorU32(ImGuiCol_Border));

                // Dragging something onto the button opens the content browser as a drop target
                auto contentBrowserHovered = RenderDrawerButton(contentBrowserWindow, IconType::Folder, "Content browser", "Ctrl+B");
                auto isDragging = ImGui::IsDragDropActive();
                drawerSpringTime = isDragging && contentBrowserHovered ? drawerSpringTime + ImGui::GetIO().DeltaTime : 0.0f;
                if (drawerSpringTime > 0.35f)
                    contentBrowserWindow.show = true;

                auto warningCount = logWindow.GetUnseenWarningCount();
                auto errorCount = logWindow.GetUnseenErrorCount();

                gap(4.0f);
                RenderDrawerButton(logWindow, IconType::Log, "Log", "Ctrl+J", errorCount, warningCount);

                gap(16.0f);

                // Context is plain information, it stays quiet next to the things that can be clicked
                auto sceneWindow = GetActiveSceneWindow();
                std::string guizmoText;
                if (sceneWindow && sceneWindow->scene.IsLoaded()) {
                    if (sceneWindow->isPlaying) {
                        RenderStatusChip("##Playing", IconType::Play, "Playing", Theme::Success(), Theme::Success(0.16f), Theme::Success(0.16f));
                        gap(8.0f);
                    }

                    auto sceneLabel = sceneWindow->IsPrefab() ? Common::Path::GetFileName(sceneWindow->prefabPath) :
                        UI::SceneWindow::GetSceneLabel(sceneWindow->scene);
                    text(sceneLabel.c_str(), ImGuiCol_Text);

                    const auto& selection = sceneWindow->sceneHierarchyPanel.selectedEntities;
                    auto selectedEntity = sceneWindow->sceneHierarchyPanel.selectedEntity;
                    if (!selection.empty()) {
                        gap(6.0f);
                        text("/", ImGuiCol_TextDisabled);
                        gap(6.0f);
                        if (selection.size() == 1 && selectedEntity.IsValid()) {
                            auto nameComponent = selectedEntity.TryGetComponent<NameComponent>();
                            auto name = nameComponent ? nameComponent->name : "Entity " + std::to_string(selectedEntity);
                            text(name.c_str(), ImGuiCol_Text);
                        }
                        else {
                            text((std::to_string(selection.size()) + " entities").c_str(), ImGuiCol_Text);
                        }
                    }

                    const auto& guizmo = sceneWindow->guizmo;
                    guizmoText = guizmo.mode == ImGuizmo::OPERATION::ROTATE ? "Rotate" :
                        guizmo.mode == ImGuizmo::OPERATION::SCALE ? "Scale" : "Translate";
                    guizmoText += guizmo.objectSpace ? " \xC2\xB7 Local" : " \xC2\xB7 World";
                    if (guizmo.snappingEnabled)
                        guizmoText += " \xC2\xB7 Snapping";
                }
                else {
                    text("No scene open", ImGuiCol_TextDisabled);
                }

                // Only long running work is worth to be mentioned, short jobs would just flicker
                auto backgroundJobs = JobSystem::GetPendingJobCount(JobPriority::Medium) +
                    JobSystem::GetPendingJobCount(JobPriority::Low);
                backgroundJobTime = backgroundJobs > 0 ? backgroundJobTime + ImGui::GetIO().DeltaTime : 0.0f;
                if (backgroundJobTime > 0.5f) {
                    gap(16.0f);
                    const char* dots[] = { ".", "..", "..." };
                    char jobText[64];
                    std::snprintf(jobText, sizeof(jobText), "%d background %s%s", backgroundJobs, backgroundJobs == 1 ? "job" : "jobs",
                        dots[int32_t(ImGui::GetTime() * 2.0) % 3]);
                    text(jobText, ImGuiCol_TextDisabled);
                }

                // Gizmo mode, performance and the notifications on the right side
                auto frameTime = Clock::GetAverage() * 1000.0f;
                char rightText[128];
                std::snprintf(rightText, sizeof(rightText), "%s%s%.1f ms \xC2\xB7 %.0f fps", guizmoText.c_str(),
                    guizmoText.empty() ? "" : "     ", frameTime, frameTime > 0.0f ? 1000.0f / frameTime : 0.0f);
                auto notificationWidth = GetStatusChipWidth("", GetNotificationBadgeWidth(Notifications::GetUnreadCount()));
                auto rightWidth = ImGui::CalcTextSize(rightText).x + ImGui::GetStyle().ItemSpacing.x + notificationWidth;
                auto rightPos = ImGui::GetWindowContentRegionMax().x - rightWidth;

                // The problem nobody has seen yet takes the room in between and opens the log
                if (warningCount + errorCount > 0 && !logWindow.GetLastProblem().empty()) {
                    gap(16.0f);
                    auto maxWidth = rightPos - ImGui::GetCursorPosX() - 16.0f;
                    auto isError = logWindow.IsLastProblemError();
                    auto color = isError ? Theme::Error() : Theme::Warning();
                    auto background = isError ? Theme::Error(0.1f) : Theme::Warning(0.1f);
                    auto backgroundHovered = isError ? Theme::Error(0.22f) : Theme::Warning(0.22f);
                    if (maxWidth > 80.0f) {
                        if (RenderStatusChip("##Last problem", isError ? IconType::Error : IconType::Warning,
                            logWindow.GetLastProblem().c_str(), color, background, backgroundHovered, maxWidth))
                            logWindow.show = true;
                        ImGui::SetItemTooltip("%s\nClick to open the log", logWindow.GetLastProblem().c_str());
                    }
                }

                if (rightPos > ImGui::GetCursorPosX()) {
                    ImGui::SameLine(rightPos);
                    text(rightText, ImGuiCol_TextDisabled);
                }

                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - notificationWidth);
                RenderNotificationButton();

                ImGui::EndMenuBar();
            }
        }
        // Side bars always need to be ended
        ImGui::End();
        ImGui::PopStyleColor();

    }

    bool App::RenderStatusChip(const char* id, IconType icon, const char* text, ImVec4 color,
        ImVec4 background, ImVec4 backgroundHovered, float maxWidth, float trailingWidth) {

        const float padding = 8.0f;
        // Chips without text only show their icon
        const float iconGap = text[0] ? 5.0f : 0.0f;

        auto fontSize = ImGui::GetFontSize();
        auto iconSize = fontSize;
        auto textSize = ImGui::CalcTextSize(text);
        auto width = std::min(GetStatusChipWidth(text, trailingWidth), maxWidth);
        auto height = ImGui::GetFrameHeight();

        // Pressing on click, since a drawer that is open closes itself when the mouse goes down outside of it
        auto pos = ImGui::GetCursorScreenPos();
        auto pressed = ImGui::InvisibleButton(id, ImVec2(width, height), ImGuiButtonFlags_PressedOnClick);
        auto hovered = ImGui::IsItemHovered();

        auto drawList = ImGui::GetWindowDrawList();
        auto min = ImVec2(pos.x, pos.y + 2.0f);
        auto max = ImVec2(pos.x + width, pos.y + height - 2.0f);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? backgroundHovered : background), 5.0f);

        auto centerY = (min.y + max.y) * 0.5f;
        auto iconMin = ImVec2(min.x + padding, centerY - iconSize * 0.5f);
        auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->GetTintable(icon));
        drawList->AddImage(set, iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize),
            ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), ImGui::GetColorU32(color));

        auto textMin = ImVec2(iconMin.x + iconSize + iconGap, GetCapCenteredTextY(centerY));
        auto textMaxX = max.x - padding - trailingWidth;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::RenderTextEllipsis(drawList, textMin, ImVec2(textMaxX, max.y), textMaxX, textMaxX, text, nullptr, &textSize);
        ImGui::PopStyleColor();

        return pressed;

    }

    float App::GetStatusChipWidth(const char* text, float trailingWidth) {

        // Padding, icon, the gap to the text and the text, chips without text only show their icon
        auto iconGap = text[0] ? 5.0f : 0.0f;
        return 8.0f + ImGui::GetFontSize() + iconGap + ImGui::CalcTextSize(text).x + trailingWidth + 8.0f;

    }

    float App::GetNotificationBadgeWidth(int32_t unreadCount) {

        // Gap to the icon and the padded count
        return unreadCount > 0 ? 6.0f + ImGui::CalcTextSize(std::to_string(unreadCount).c_str()).x + 10.0f : 0.0f;

    }

    void App::RenderNotificationButton() {

        auto unreadCount = Notifications::GetUnreadCount();
        auto hasError = unreadCount > 0 && Notifications::HasUnreadError();
        auto countText = std::to_string(unreadCount);
        auto badgeWidth = GetNotificationBadgeWidth(unreadCount);

        auto active = notificationsOpen || unreadCount > 0;
        auto color = active ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : Theme::TextMuted();
        auto background = notificationsOpen ? Theme::Accent(0.35f) : Theme::TextMuted(0.12f);
        auto backgroundHovered = notificationsOpen ? Theme::Accent(0.45f) : Theme::TextMuted(0.22f);

        // The popup closes itself when the mouse goes down outside of it, a click on the button shouldn't open it again
        auto wasOpen = notificationsOpen;
        if (RenderStatusChip("##Notifications", IconType::Notifications, "", color, background, backgroundHovered,
            FLT_MAX, badgeWidth) && !wasOpen)
            ImGui::OpenPopup("##Notification center");
        if (unreadCount > 0)
            ImGui::SetItemTooltip("%d new %s", unreadCount, unreadCount == 1 ? "notification" : "notifications");
        else
            ImGui::SetItemTooltip("Notifications");

        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();
        if (unreadCount > 0) {
            auto badgeColor = hasError ? Theme::Error() : Theme::Accent();
            auto min = ImVec2(itemMax.x - 8.0f - badgeWidth + 6.0f, itemMin.y + 5.0f);
            auto max = ImVec2(itemMax.x - 8.0f, itemMax.y - 5.0f);
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(badgeColor), (max.y - min.y) * 0.5f);
            drawList->AddText(ImVec2(min.x + 5.0f, GetCapCenteredTextY((min.y + max.y) * 0.5f)),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), countText.c_str());
        }

        ImGui::SetNextWindowPos(ImVec2(itemMax.x, itemMin.y - 6.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        notificationsOpen = ImGui::BeginPopup("##Notification center");
        if (notificationsOpen) {
            // Everything that is visible while it is open counts as read
            Notifications::MarkAsRead();
            Notifications::DisplayHistory();
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

    }

    float App::GetCapCenteredTextY(float centerY) {

        // The line height includes room for descenders, labels look centered when their capitals are
        auto font = ImGui::GetFont();
        auto scale = ImGui::GetFontSize() / font->FontSize;
        auto glyph = font->FindGlyph('H');
        return centerY - (glyph->Y0 + glyph->Y1) * 0.5f * scale;

    }

    bool App::RenderDrawerButton(UI::DrawerWindow& drawer, IconType icon, const char* label, const char* shortcut,
        int32_t errorCount, int32_t warningCount) {

        const float badgeGap = 6.0f;
        const float badgePadding = 5.0f;
        const float badgeIconGap = 3.0f;

        auto color = drawer.show ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : Theme::TextMuted();
        auto background = drawer.show ? Theme::Accent(0.35f) : Theme::TextMuted(0.12f);
        auto backgroundHovered = drawer.show ? Theme::Accent(0.45f) : Theme::TextMuted(0.22f);

        // New warnings and errors are shown as badges inside of the button
        struct Badge {
            IconType icon;
            ImVec4 color;
            int32_t count;
            const char* name;
            char text[16];
            float width;
        };
        Badge badges[] = {
            { IconType::Error, Theme::Error(), errorCount, "errors" },
            { IconType::Warning, Theme::Warning(), warningCount, "warnings" }
        };

        auto badgeIconSize = ImGui::GetFontSize() * 0.9f;
        auto trailingWidth = 0.0f;
        for (auto& badge : badges) {
            if (badge.count <= 0)
                continue;
            std::snprintf(badge.text, sizeof(badge.text), "%d", badge.count);
            badge.width = badgePadding + badgeIconSize + badgeIconGap + ImGui::CalcTextSize(badge.text).x + badgePadding;
            trailingWidth += badgeGap + badge.width;
        }

        if (RenderStatusChip(label, icon, label, color, background, backgroundHovered, FLT_MAX, trailingWidth))
            drawer.show = !drawer.show;
        auto hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        auto drawList = ImGui::GetWindowDrawList();
        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();
        auto fontSize = ImGui::GetFontSize();
        auto x = itemMax.x - 8.0f - trailingWidth;
        const Badge* hoveredBadge = nullptr;
        for (const auto& badge : badges) {
            if (badge.count <= 0)
                continue;
            x += badgeGap;
            auto min = ImVec2(x, itemMin.y + 5.0f);
            auto max = ImVec2(x + badge.width, itemMax.y - 5.0f);
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(badge.color.x, badge.color.y, badge.color.z, 0.2f)), 4.0f);

            auto iconMin = ImVec2(min.x + badgePadding, (itemMin.y + itemMax.y - badgeIconSize) * 0.5f);
            auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->GetTintable(badge.icon));
            drawList->AddImage(set, iconMin, ImVec2(iconMin.x + badgeIconSize, iconMin.y + badgeIconSize),
                ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), ImGui::GetColorU32(badge.color));
            drawList->AddText(ImVec2(iconMin.x + badgeIconSize + badgeIconGap, GetCapCenteredTextY((itemMin.y + itemMax.y) * 0.5f)),
                ImGui::GetColorU32(badge.color), badge.text);

            if (hovered && ImGui::IsMouseHoveringRect(min, max))
                hoveredBadge = &badge;
            x += badge.width;
        }

        if (hoveredBadge)
            ImGui::SetItemTooltip("%d new %s", hoveredBadge->count, hoveredBadge->name);
        else
            ImGui::SetItemTooltip("Toggle %s (%s)", label, shortcut);

        return hovered;

    }

    void App::UpdateDrawers() {

        // Only one drawer is open at a time, the last one opened replaces the other one
        if (contentBrowserWindow.IsOpening() && logWindow.show && !logWindow.pinned)
            contentBrowserWindow.TakeOver(logWindow);
        else if (logWindow.IsOpening() && contentBrowserWindow.show && !contentBrowserWindow.pinned)
            logWindow.TakeOver(contentBrowserWindow);

        // The source item of a drag isn't rendered anymore after the drawer closed
        auto payload = ImGui::GetDragDropPayload();
        if (payload && payload->IsDataType("ContentBrowserResource") && !contentBrowserWindow.IsVisible() &&
            ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
            ImGui::TextUnformatted(Common::Path::GetFileName(static_cast<const char*>(payload->Data)).c_str());
            ImGui::TextDisabled("Drop on a matching resource slot");
            ImGui::EndDragDropSource();
        }

    }

    void App::PrepareDrawerDocking(ImGuiID dsID) {

        // Pinned drawers without a place in the saved layout are docked again
        auto requestDockingIfLost = [](UI::DrawerWindow& drawer) {
            if (!drawer.pinned || drawer.requestDocking || ImGui::FindWindowByName(drawer.GetNameID()))
                return;
            auto settings = ImGui::FindWindowSettingsByID(ImHashStr(drawer.GetNameID()));
            drawer.requestDocking = !settings || !settings->DockId;
            };
        requestDockingIfLost(contentBrowserWindow);
        requestDockingIfLost(logWindow);

        drawerDockNodeID = 0;
        if (!contentBrowserWindow.requestDocking && !logWindow.requestDocking)
            return;

        // A drawer that is docked already shares its node with the new one
        auto getDockID = [](const UI::DrawerWindow& drawer) -> ImGuiID {
            auto window = ImGui::FindWindowByName(drawer.GetNameID());
            return drawer.pinned && !drawer.requestDocking && window ? window->DockId : 0;
            };
        drawerDockNodeID = getDockID(contentBrowserWindow) ? getDockID(contentBrowserWindow) : getDockID(logWindow);
        if (drawerDockNodeID)
            return;

        auto centralNode = ImGui::DockBuilderGetCentralNode(dsID);
        if (!centralNode)
            return;

        ImGuiID dockIdUp;
        ImGui::DockBuilderSplitNode(centralNode->ID, ImGuiDir_Down, 0.3f, &drawerDockNodeID, &dockIdUp);
        ImGui::DockBuilderFinish(dsID);

    }

    void App::DockPinnedDrawer(UI::DrawerWindow& drawer) {

        // A new layout has its central node only after the first update, the request waits until then
        if (!drawer.requestDocking || !drawerDockNodeID)
            return;
        drawer.requestDocking = false;

        ImGui::SetNextWindowDockID(drawerDockNodeID, ImGuiCond_Always);

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
                { "Ctrl+B", "Toggle the content browser" },
                { "Ctrl+J", "Toggle the log" },
                { "Esc", "Close the open drawer" },
                { "Ctrl+S", "Save the active scene" },
                { "Ctrl+Z", "Undo in the scene or asset editor in front" },
                { "Ctrl+Shift+Z / Y", "Redo" },
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
            section("Material graph", {
                { "Space or right click", "Add a node, dropping a link on the canvas connects it" },
                { "Middle or right mouse drag", "Move the canvas" },
                { "Mouse wheel", "Zoom" },
                { "Drag a pin", "Connect, drag an input to move its link" },
                { "Alt+Click on a link", "Remove the link" },
                { "Ctrl+C / V / D", "Copy, paste or duplicate the selection" },
                { "F", "Show all nodes" },
                { "Delete", "Delete the selection" },
                });
        }
        ImGui::End();

    }

    void App::SetupMainDockspace(ImGuiID dsID) {

        ImGui::SetWindowSize(ImVec2(370.0f, 680.0f));
        ImGui::SetWindowPos(ImVec2(640.0f, 55.0f));

        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dsID, ImGui::GetMainViewport()->WorkSize);

        for (auto& sceneWindow : sceneWindows) {
            ImGui::DockBuilderDockWindow(sceneWindow->GetNameID(), dsID);
        }

        ImGui::DockBuilderFinish(dsID);

        profilerWindow.show = false;
        settingsWindow.show = false;
        // Pinned drawers keep their place in the new layout
        contentBrowserWindow.show = contentBrowserWindow.requestDocking = contentBrowserWindow.pinned;
        logWindow.show = logWindow.requestDocking = logWindow.pinned;

        for (const auto& sceneWindow : sceneWindows) {
            sceneWindow->resetDockingLayout = true;
            sceneWindow->show = true;
        }

        resetDockspaceLayout = false;
        dockLoadingScenes = true;

    }

    void App::OpenInAssetEditor(const std::string& path) {

        auto type = Content::contentTypeMapping.at(Common::Path::GetFileType(path));
        if (type == ContentType::Animation) {
            // Animations play in the mesh editor focused last
            Ref<UI::MeshEditorWindow> assetEditorWindow;
            for (const auto& window : assetEditorWindows) {
                auto candidate = std::dynamic_pointer_cast<UI::MeshEditorWindow>(window);
                if (candidate && (!assetEditorWindow || candidate->lastFocusTime > assetEditorWindow->lastFocusTime))
                    assetEditorWindow = candidate;
            }
            auto animation = FileImporter::ImportFile<Animation::Animation>(path);
            if (!assetEditorWindow || !assetEditorWindow->Play(animation))
                Notifications::Push({ .message = "Open a skinned mesh to play the animation on", .type = NotificationType::Error });
            return;
        }

        if (type == ContentType::Prefab) {
            for (const auto& sceneWindow : sceneWindows) {
                if (sceneWindow->prefabPath == path)
                    return;
            }

            auto scene = DataCreator::CreateScene(Common::Path::GetFileName(path), vec3(-2048.0f), vec3(2048.0f), 8);
            auto sceneWindow = CreateRef<UI::SceneWindow>(ResourceHandle<Scene::Scene>(scene), true);
            sceneWindow->OpenPrefab(path);
            OpenAsTab(*sceneWindow);
            sceneWindows.push_back(sceneWindow);
            return;
        }

        // Scenes open as a scene tab, like a double click in the content browser
        if (type == ContentType::Scene) {
            FileImporter::ImportFile<Scene::Scene>(path);
            return;
        }

        if (type != ContentType::Material && type != ContentType::MaterialGraph && type != ContentType::Mesh) {
            Notifications::Push({ .message = "There is no editor for " + Common::Path::GetFileName(path), .type = NotificationType::Warning });
            return;
        }

        auto iter = std::find_if(assetEditorWindows.begin(), assetEditorWindows.end(),
            [&](const auto& window) { return window->GetAssetPath() == path; });
        if (iter != assetEditorWindows.end()) {
            (*iter)->requestFocus = true;
            activeAssetEditorWindow = *iter;
            return;
        }

        if (type == ContentType::Material)
            assetEditorWindows.push_back(CreateRef<UI::MaterialEditorWindow>(FileImporter::ImportFile<Material>(path), true));
        else if (type == ContentType::MaterialGraph)
            assetEditorWindows.push_back(CreateRef<UI::MaterialEditorWindow>(FileImporter::ImportFile<MaterialGraph::Graph>(path), true));
        else
            assetEditorWindows.push_back(CreateRef<UI::MeshEditorWindow>(FileImporter::ImportFile<Mesh::Mesh>(path), true));
        // Docks next to the window which is still the active one
        OpenAsTab(*assetEditorWindows.back());
        activeAssetEditorWindow = assetEditorWindows.back();

    }

    void App::DockNextToScenes(UI::Window& window) {

        if (!window.requestDocking)
            return;
        window.requestDocking = false;

        if (window.dockNodeID && ImGui::DockBuilderGetNode(window.dockNodeID)) {
            ImGui::SetNextWindowDockID(window.dockNodeID, ImGuiCond_Always);
            window.dockNodeID = 0;
            return;
        }
        window.dockNodeID = 0;

        for (const auto& sceneWindow : sceneWindows) {
            auto imguiWindow = ImGui::FindWindowByName(sceneWindow->GetNameID());
            if (sceneWindow.get() != &window && imguiWindow && imguiWindow->DockId) {
                ImGui::SetNextWindowDockID(imguiWindow->DockId, ImGuiCond_Always);
                return;
            }
        }

        // Nothing to join, so the window takes the free space of the layout
        auto centralNode = ImGui::DockBuilderGetCentralNode(ImGui::GetID("MainDS"));
        if (centralNode)
            ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_Always);

    }

    ImGuiID App::GetActiveDockNodeID() const {

        const UI::Window* window = activeAssetEditorWindow.get();
        if (!window && !sceneWindows.empty())
            window = sceneWindows[std::min(activeSceneIdx, sceneWindows.size() - 1)].get();

        auto imguiWindow = window ? ImGui::FindWindowByName(window->GetNameID()) : nullptr;
        return imguiWindow ? imguiWindow->DockId : 0;

    }

    void App::OpenAsTab(UI::Window& window) const {

        window.requestDocking = true;
        window.dockNodeID = GetActiveDockNodeID();
        window.requestFocus = true;

    }

    void App::SubscribeToResourceEvents() {

        ResourceManager<Scene::Scene>::Subscribe(ResourceTopic::ResourceCreate,
            [&](Ref<Resource<Scene::Scene>>& scene) {
                waitToLoadScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                if (!restoringScenes)
                    userOpenedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->openedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->AddRecentScene(scene->path);
            });

        // Also kind of a resource event
        Events::EventManager::DropEventDelegate.Subscribe([&](Events::DropEvent event) {
            if (!event.file.empty()) {
                contentBrowserWindow.show = true;
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
