#include "App.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Serialization.h"
#include "Notifications.h"
#include "ContentDiscovery.h"
#include "Theme.h"
#include "DataCreator.h"
#include "RecentProjects.h"

#include <Processes.h>
#include "ui/panels/PopupPanels.h"
#include "tools/FileSystemHelper.h"
#include "tools/AssetReferenceIndex.h"
#include "tools/AssetRenamer.h"
#include <ImGuizmo.h>
#include <profiler/CPUProfiler.h>
#include <profiler/HitchRecorder.h>
#include <streaming/StreamingManager.h>
#include <Serializer.h>
#include <scripting/BindingRegistry.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
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

        if (engineDataDirectory.empty())
            engineDataDirectory = Loader::AssetLoader::GetDataDirectory();
        LoadProject();

        ContentDiscovery::Update();

        // The ImGui wrapper creates the ImPlot context
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // Each project keeps its own layout
        if (Singletons::project) {
            imguiIniPath = Singletons::project->GetDirectory() + "/.atlas/imgui.ini";
            io.IniFilename = imguiIniPath.c_str();
        }

        if (!switchingProject)
            SetDefaultWindowResolution();

        auto icon = Atlas::Texture::Texture2D("icon.png");
        window.SetIcon(&icon);

        SubscribeToResourceEvents();

        Singletons::imguiWrapper = CreateRef<ImguiExtension::ImguiWrapper>();
        Singletons::config = CreateRef<Config>();

        Serialization::DeserializeConfig();

        // --log-hitches [ms] logs frames that take much longer than the ones before with their CPU profile, it
        // overrides the hitch settings of the config for this run
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] != "--log-hitches")
                continue;
            auto hitchSettings = HitchRecorder::GetSettings();
            configHitchSettings = hitchSettings;
            hitchSettings.enable = true;
            hitchSettings.cpu.enable = true;
            hitchSettings.log = true;
            if (i + 1 < args.size() && std::isdigit(static_cast<unsigned char>(args[i + 1][0])))
                hitchSettings.cpu.minimumTime = std::stof(args[i + 1]);
            HitchRecorder::SetSettings(hitchSettings);
        }

        workspace.contentBrowserWindow.currentDirectory = Loader::AssetLoader::GetDataDirectory();
        Serialization::DeserializeContentBrowserWindow(workspace.contentBrowserWindow);

        Serialization::DeserializeDrawerWindow(workspace.logWindow);
        Serialization::DeserializeDrawerWindow(workspace.profilerWindow);
        restoringScenes = false;

        // A hand edited config may name a drawer twice or one which doesn't exist
        auto& statusBarDrawers = Singletons::config->statusBarDrawers;
        std::vector<std::string> knownDrawers;
        for (const auto& name : statusBarDrawers)
            if (workspace.FindDrawer(name) && std::find(knownDrawers.begin(), knownDrawers.end(), name) == knownDrawers.end())
                knownDrawers.push_back(name);
        statusBarDrawers = knownDrawers;

        // The font depends on the scale from the config, its resources are created only once
        UpdateUiScale();
        Singletons::imguiWrapper->Load(&window);

        // Everything that needs the config comes afterwards
        Singletons::icons = CreateRef<Icons>();
        Singletons::blockingOperation = CreateRef<BlockingOperation>();
        Singletons::contentPreviews = CreateRef<ContentPreviews>();
        Singletons::mainRenderer = mainRenderer;

        mouseHandler = Input::MouseHandler(1.5f, 8.0f);
        keyboardHandler = Input::KeyboardHandler(7.0f, 5.0f);

        ApplyTheme();

        Singletons::commandPalette = CreateRef<UI::CommandPalette>();
        editorCommands.Register();
        RegisterCommands();

        projectHub = std::make_unique<UI::ProjectHubWindow>(engineDataDirectory,
            (std::filesystem::path(engineDataDirectory).parent_path() / "templates").generic_string());

        if (Singletons::project) {
            workspace.projectSettingsWindow.onChange = [this](const Project&) { ApplyProjectSettings(); };
            ApplyProjectSettings();
            OpenStartScene();
            // --mcp-port <port> if another program uses the default port
            auto port = Bridge::McpServer::defaultPort;
            for (size_t i = 0; i + 1 < args.size(); i++) {
                if (args[i] == "--mcp-port")
                    port = std::stoi(args[i + 1]);
            }
            bridge = std::make_unique<EditorBridge>(Singletons::project, port, [this]() { return workspace.GetActiveSceneWindow(); });
            bridge->addEditorFunctions = [this](Scripting::Binder& editor) { editorFunctions.Add(editor); };
            // Recorded for the bindings and completions before the first script runs
            sol::state recorder;
            Scripting::Binder editorTable(recorder.create_table(), "Editor");
            editorFunctions.Add(editorTable);
            // Lua of the agent changes the project like the settings window: applied, undoable there and saved by the user
            bridge->onProjectChange = [this](const Project& before) {
                workspace.projectSettingsWindow.RecordChange(Singletons::project, before, "project (Lua)");
                };
        }

    }

    void App::RegisterCommands() {

        auto& config = Singletons::config;
        auto& palette = Singletons::commandPalette;

        palette->Register({ .category = "File", .name = "New project...", .action = [this]() { projectHub->OpenNewProjectPopup(); } });
        palette->Register({ .category = "File", .name = "Open project...", .action = [this]() { OpenProject(""); } });
        palette->Register({ .category = "File", .name = "Export Lua annotations", .action = [this]() { ExportLuaAnnotations(); } });
        palette->Register({ .category = "Window", .name = "Reset layout", .action = [this]() { resetDockspaceLayout = true; } });
        palette->Register({ .category = "View", .name = "Toggle dark mode",
            .action = [this, &config]() { config->darkMode = !config->darkMode; ApplyTheme(); },
            .isChecked = [&config]() { return config->darkMode; } });
        palette->Register({ .category = "Help", .name = "Keyboard shortcuts", .action = [this]() { showShortcutsWindow = true; } });

    }

    void App::LoadProject() {

        std::string path;
        for (size_t i = 0; i + 1 < args.size(); i++) {
            if (args[i] == "--project")
                path = args[i + 1];
        }

        // main() changes into the directory of the executable
        auto project = path.empty() ? nullptr : Project::Load(Bridge::ResolveUserPath(path).string());
        if (!project) {
            // The hub works with the data of the engine, e.g. after closing a project
            Project::Activate(nullptr);
            Loader::AssetLoader::SetAssetDirectory(engineDataDirectory);
            // The project hub only keeps settings like the interface scale, it opens no scenes
            auto prefPath = SDL_GetPrefPath("AtlasEngine", "Editor");
            if (prefPath)
                Serialization::SetConfigDirectory(prefPath);
            SDL_free(prefPath);
            return;
        }

        Project::Activate(project);

        // Asset directories which were used without a project kept the editor state in .config
        auto stateDirectory = std::filesystem::path(project->GetDirectory()) / ".atlas";
        auto legacyDirectory = std::filesystem::path(project->GetAssetDirectory()) / ".config";
        std::error_code error;
        if (!std::filesystem::exists(stateDirectory, error) && std::filesystem::is_directory(legacyDirectory, error))
            std::filesystem::copy(legacyDirectory, stateDirectory, std::filesystem::copy_options::recursive, error);
        Serialization::SetConfigDirectory(stateDirectory.generic_string());

        RecentProjects::Add(*project);
        window.SetTitle("Atlas Engine Editor - " + project->name);
        Singletons::project = project;

    }

    void App::ApplyProjectSettings() {

        auto& project = Singletons::project;
        project->ApplySettings();
        // Meshes and impostors are saved and baked here, they stay resident whatever the project sets
        Streaming::StreamingManager::settings.impostors.enable = false;
        Streaming::StreamingManager::settings.meshes.enable = false;

        for (const auto& sceneWindow : workspace.sceneWindows) {
            if (sceneWindow->scene.IsLoaded())
                project->ApplyTo(*sceneWindow->scene.Get());
        }

    }

    void App::ExportLuaAnnotations() {

        auto directory = std::filesystem::path(Singletons::project->GetDirectory());
        auto file = directory / ".atlas" / "lua" / "atlas.lua";
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << Scripting::BindingRegistry::ToLuaAnnotations();

        auto luarc = directory / ".luarc.json";
        if (!std::filesystem::exists(luarc))
            std::ofstream(luarc, std::ios::binary) << json({ { "workspace.library", { ".atlas/lua" } } }).dump(2);
        Notifications::Push({ .message = "Lua annotations written to " + file.generic_string() });

    }

    void App::OpenStartScene() {

        auto& project = Singletons::project;
        if (project->startScene.empty() || !Singletons::config->openedScenes.empty())
            return;

        if (!Loader::AssetLoader::FileExists(project->startScene)) {
            auto scene = DataCreator::CreateScene(Common::Path::GetFileNameWithoutExtension(project->startScene),
                vec3(-2048.0f), vec3(2048.0f), 8);

            // Playing needs a main camera
            auto camera = scene->CreateEntity();
            camera.AddComponent<NameComponent>("Camera");
            camera.AddComponent<TransformComponent>(glm::translate(mat4(1.0f), vec3(0.0f, 2.0f, 10.0f)));
            auto& cameraComponent = camera.AddComponent<CameraComponent>(47.0f, 2.0f, 0.5f, 2000.0f);
            cameraComponent.isMain = true;
            scene->GetRootEntity().GetComponent<HierarchyComponent>().AddChild(camera);

            Serializer::SerializeScene(scene, project->startScene, false, true, false);
        }

        FileImporter::ImportFile<Scene::Scene>(project->startScene);

    }

    void App::OpenProject(const std::string& path) {

        // Unsaved changes are asked about first, like when the editor closes
        nextProject = path;
        if (AllowQuit())
            projectSwitchRequested = true;

    }

    void App::SwitchProject() {

        WaitForRender();
        auto path = *nextProject;
        UnloadContent();

        workspace.Close();
        waitToLoadScenes.clear();
        userOpenedScenes.clear();
        unsavedWindows.clear();
        emptyStateScenes.clear();
        quitAfterUnsavedChanges = false;
        quitConfirmed = false;
        systemUiScale = 0.0f;
        fontRasterSize = 0.0f;
        emptyStateVisible = false;
        resetDockspaceLayout = false;
        dockLoadingScenes = false;
        restoringScenes = true;
        projectHub.reset();
        nextProject.reset();
        projectSwitchRequested = false;

        // The project replaces the one the editor started with, without one the project hub opens
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "--project" && i + 1 < args.size()) {
                args.erase(args.begin() + i, args.begin() + i + 2);
                break;
            }
        }
        if (!path.empty()) {
            args.push_back("--project");
            args.push_back(path);
        }

        switchingProject = true;
        LoadContent();
        switchingProject = false;

    }

    void App::UnloadContent() {

        Loader::ModelImporter::WaitForBackgroundImports();

        Singletons::imguiWrapper->Unload();

        CopyPasteHelper::Clear();

        for (const auto& sceneWindow : workspace.sceneWindows) {
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            // There is no update anymore which would run the queued operations
            Singletons::blockingOperation->Finish();
        }

        if (configHitchSettings)
            HitchRecorder::SetSettings(*configHitchSettings);
        configHitchSettings.reset();
        SaveEditorState();
        AssetReferenceIndex::Shutdown();
        AssetRenamer::Shutdown();
        ContentDiscovery::Shutdown();

        bridge.reset();
        Singletons::Destruct();

        ResourceManager<Scene::Scene>::Unsubscribe(ResourceTopic::ResourceCreate, sceneCreateSubscription);
        Events::EventManager::DropEventDelegate.Unsubscribe(dropSubscription);

        ImGui::DestroyContext();

    }

    void App::SaveEditorState() {

        // The editor camera doesn't exist while playing
        for (const auto& sceneWindow : workspace.sceneWindows)
            if (!sceneWindow->IsPrefab() && !sceneWindow->isPlaying)
                Serialization::SerializeSceneWindow(sceneWindow);

        Serialization::SerializeContentBrowserWindow(workspace.contentBrowserWindow);
        Serialization::SerializeDrawerWindow(workspace.logWindow);
        Serialization::SerializeDrawerWindow(workspace.profilerWindow);
        if (!configHitchSettings)
            Serialization::SerializeConfig();

        lastEditorStateSaveTime = Clock::Get();

    }

    void App::Update(float deltaTime) {

        if (projectSwitchRequested)
            SwitchProject();

        const ImGuiIO& io = ImGui::GetIO();

        CPUProfiler::Scope sectionScope("Content discovery");
        // Will lead to frame time spikes, don't do it if content can't be seen
        if (!workspace.IsPlayingMaximized())
            ContentDiscovery::Update();
        sectionScope.Next("ImGui update");
        UpdateUiScale();
        Singletons::imguiWrapper->Update(&window, deltaTime);

        Singletons::blockingOperation->Update();

        // Commands wait while playing starts or stops
        if (bridge && !Singletons::blockingOperation->block) {
            sectionScope.Next("Bridge commands");
            bridge->Update();
        }

        if (Singletons::blockingOperation->block)
            return;

        if (Clock::Get() - lastEditorStateSaveTime > editorStateSaveInterval)
            SaveEditorState();

        Loader::ModelImporter::ApplyBackgroundImports();

        sectionScope.Next("Content previews update");
        // Also while no previews are shown, they release what they don't need anymore
        Singletons::contentPreviews->ReleaseUnused();

        sectionScope.Next("Content browser update");
        workspace.contentBrowserWindow.Update();
        workspace.logWindow.Update();

        // Requests to show a file also have to work while the drawer is closed
        if (!UI::ContentBrowserWindow::contentToShowPath.empty())
            workspace.contentBrowserWindow.show = true;

        if (!UI::ContentBrowserWindow::contentToEditPath.empty()) {
            auto path = UI::ContentBrowserWindow::contentToEditPath;
            workspace.Open(path);
            for (const auto& codeEditorWindow : workspace.codeEditorWindows)
                if (codeEditorWindow->GetPath() == path && UI::ContentBrowserWindow::contentToEditLine > 0)
                    codeEditorWindow->codeEditor.GoToLine(UI::ContentBrowserWindow::contentToEditLine);
            UI::ContentBrowserWindow::contentToEditPath.clear();
            UI::ContentBrowserWindow::contentToEditLine = 0;
        }

        // Windows with unsaved changes stay open until the user decided about them
        for (const auto& assetEditorWindow : workspace.assetEditorWindows) {
            if (!assetEditorWindow->show && assetEditorWindow->hasUnsavedChanges) {
                assetEditorWindow->show = true;
                AddUnsavedWindow(assetEditorWindow);
            }
        }
        std::erase_if(workspace.assetEditorWindows, [](const auto& window) { return !window->show; });
        if (workspace.activeAssetEditorWindow && !workspace.activeAssetEditorWindow->show)
            workspace.activeAssetEditorWindow = nullptr;
        for (const auto& assetEditorWindow : workspace.assetEditorWindows)
            if (assetEditorWindow->inFocus) {
                workspace.activeAssetEditorWindow = assetEditorWindow;
                workspace.activeCodeEditorWindow = nullptr;
            }
        // Drags outside of viewports, e.g. on a slider, must not turn the next camera that gets the focus
        auto mouseMovedCamera = false;
        for (const auto& assetEditorWindow : workspace.assetEditorWindows) {
            if (!assetEditorWindow->viewportPanel.isFocused || !assetEditorWindow->cameraEntity.IsValid() || ImGuizmo::IsUsing())
                continue;
            auto& camera = assetEditorWindow->cameraEntity.GetComponent<CameraComponent>();
            keyboardHandler.speed = assetEditorWindow->cameraMovementSpeed;
            mouseHandler.Update(camera, deltaTime);
            keyboardHandler.Update(camera, deltaTime);
            mouseMovedCamera = true;
        }

        sectionScope.Next("Scene windows update");

        if (Singletons::project)
            Singletons::commandPalette->HandleShortcuts();

        // Create new windows for fully loaded scenes
        for (size_t i = 0; i < waitToLoadScenes.size(); i++) {
            if (waitToLoadScenes[i].IsLoaded()) {
                std::swap(waitToLoadScenes[i], waitToLoadScenes.back());

                // Projects saved before they had the scene settings take them from their first scene
                auto& project = Singletons::project;
                if (project->AdoptSceneSettings(*waitToLoadScenes.back().Get()))
                    project->Save();
                project->ApplyTo(*waitToLoadScenes.back().Get());

                // After the scene is loaded we can retrieve the window configuration and deserialize it
                workspace.sceneWindows.emplace_back(Serialization::DeserializeSceneWindow(waitToLoadScenes.back()));
                // Scenes opened by the user and the ones opened for the first time join the others, restored
                // ones stay where they were left and after a layout reset nobody has a place anymore
                auto settings = ImGui::FindWindowSettingsByID(ImHashStr(workspace.sceneWindows.back()->GetNameID()));
                workspace.sceneWindows.back()->requestDocking = !settings || dockLoadingScenes;
                // Large scenes take a while, it's worth to say when they are there
                if (std::erase(userOpenedScenes, waitToLoadScenes.back())) {
                    workspace.OpenAsTab(*workspace.sceneWindows.back());
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
        if (workspace.sceneWindows.empty()) {
            if (!mouseMovedCamera)
                mouseHandler.Reset();
            ImGuizmo::Enable(true);
            return;
        }

        auto& config = Singletons::config;

        std::vector<size_t> toBeDeletedSceneWindows;

        // Find active scene and old scene windows
        size_t sceneCounter = 0;
        for (const auto& sceneWindow : workspace.sceneWindows) {

            // Delete windows not opened in the config (don't know when this does happen), prefabs aren't part of it
            if (!sceneWindow->IsPrefab() && std::find(config->openedScenes.begin(), config->openedScenes.end(),
                sceneWindow->scene) == config->openedScenes.end()) {
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            // Windows with unsaved changes stay open until the user decided about them
            if (!sceneWindow->show && sceneWindow->hasUnsavedChanges) {
                sceneWindow->show = true;
                AddUnsavedWindow(sceneWindow);
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
                workspace.activeSceneIdx = sceneCounter;
                workspace.activeAssetEditorWindow = nullptr;
                workspace.activeCodeEditorWindow = nullptr;
            }

            sceneCounter++;
        }

        // Reverse such that indices remain valid
        std::reverse(toBeDeletedSceneWindows.begin(), toBeDeletedSceneWindows.end());
        for (auto sceneWindowIdx : toBeDeletedSceneWindows) {
            // Serialize window before erasing it
            if (workspace.sceneWindows[sceneWindowIdx]->isPlaying)
                workspace.sceneWindows[sceneWindowIdx]->StopPlaying();
            // Queued operations reference the window
            Singletons::blockingOperation->Finish();
            if (!workspace.sceneWindows[sceneWindowIdx]->IsPrefab())
                Serialization::SerializeSceneWindow(workspace.sceneWindows[sceneWindowIdx]);
            workspace.sceneWindows.erase(workspace.sceneWindows.begin() + sceneWindowIdx);
        }

        // Avoid crash when nothing is there anymore
        if (workspace.sceneWindows.empty())
            return;

        // Update active scene with input
        workspace.activeSceneIdx = std::min(workspace.activeSceneIdx, workspace.sceneWindows.size() - 1);

        auto& activeSceneWindow = workspace.sceneWindows[workspace.activeSceneIdx];

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

        // Scenes which play without a player keep the camera of the editor
        auto playCameraEntity = activeSceneWindow->playCameraEntity;
        if (activeSceneWindow->isPlaying && playCameraEntity.IsValid() && !lockMovement &&
            (activeSceneWindow->viewportPanel.isFocused || workspace.IsPlayingMaximized())) {
            auto& camera = playCameraEntity.GetComponent<CameraComponent>();
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
        for (auto& sceneWindow : workspace.sceneWindows) {
            // Only the last focused scene uses the camera input and the gizmo
            if (sceneWindow == activeSceneWindow && !workspace.activeAssetEditorWindow)
                sceneWindow->isActiveWindow = true;
            else
                sceneWindow->isActiveWindow = false;

            // Need to reset this each frame in order to reenable selection
            sceneWindow->Update(deltaTime);
        }

        // Asset editors have their own gizmos, e.g. for the pins of soft bodies
        ImGuizmo::Enable(workspace.activeAssetEditorWindow || activeSceneWindow->guizmo.needEnabled);

    }

    void App::Render(float deltaTime) {

        graphicsDevice->WaitForPreviousFrameSubmission();

        auto forEachScene = [&](const auto& function) {

            for (const auto& sceneWindow : workspace.sceneWindows) {
                if (sceneWindow->scene.IsLoaded())
                    function(*sceneWindow->scene.Get());
            }

            // The preview scenes of asset editors are updated even if their viewport isn't rendered
            for (const auto& assetEditorWindow : workspace.assetEditorWindows) {
                if (assetEditorWindow->scene)
                    function(*assetEditorWindow->scene);
            }

        };

        // Playing scenes were updated in Update(), the panels below change what their jobs read. Their render lists
        // are still to be rendered in this frame, they are only dropped when nothing is rendered
        auto occluded = window.IsOccluded();
        forEachScene([&](Scene::Scene& scene) {
            if (occluded)
                scene.WaitForAsyncWorkCompletion();
            else
                scene.renderState.WaitForJobs();
            });
        if (occluded)
            return;

        auto& config = Singletons::config;

        auto swapChainVsync = graphicsDevice->swapChain->presentMode == VK_PRESENT_MODE_FIFO_KHR;
        if (swapChainVsync != config->vsync) {
            if (config->vsync) graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_FIFO_KHR, Atlas::Graphics::SRGB_NONLINEAR);
            else graphicsDevice->CreateSwapChain(VK_PRESENT_MODE_IMMEDIATE_KHR, Atlas::Graphics::SRGB_NONLINEAR);
        }

        CPUProfiler::Scope sectionScope("ImGui new frame");
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        if (!Singletons::project) {
            if (auto path = projectHub->Render(window.GetSDLWindow()))
                OpenProject(*path);
            Notifications::Display();
            ImGui::Render();
            Singletons::imguiWrapper->Render(true);
            return;
        }

        // ImGui::ShowDemoWindow();
        // ImPlot::ShowDemoWindow();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        auto activeSceneWindow = workspace.GetActiveSceneWindow();
        // Commands of the frame can change it
        auto playingMaximized = workspace.IsPlayingMaximized();
        if (!playingMaximized) {
            // The dockspace host is transparent, fill the gaps between the docked windows with the base color
            ImGui::GetBackgroundDrawList(viewport)->AddRectFilled(viewport->Pos,
                ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                ImGui::GetColorU32(Theme::Base()));

            // Status bar first, it reduces the work area of the viewport
            sectionScope.Next("Status bar");
            statusBar.Render();
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
            workspace.PrepareDrawerDocking(mainDsId);

            // The tabs have close buttons, a second one at the end of the tab bar would just be confusing
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton;
            ImGui::DockSpace(mainDsId, ImVec2(0.0f, 0.0f), dockspace_flags);

            // Layouts saved before the drawers existed have no central node to dock into
            if (!ImGui::DockBuilderGetCentralNode(mainDsId))
                resetDockspaceLayout = true;

            if (workspace.sceneWindows.empty())
                RenderEmptyState(mainDsId);
            else
                emptyStateVisible = false;

            sectionScope.Next("Main menu bar");
            if (ImGui::BeginMainMenuBar()) {
                static bool newScene = false;
                bool saveScene = false, exitEditor = false;
                auto getShortcut = [](const char* command) { return Singletons::commandPalette->GetShortcutText(command); };
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("New project..."))
                        projectHub->OpenNewProjectPopup();
                    if (ImGui::MenuItem("Open project..."))
                        OpenProject("");
                    ImGui::SetItemTooltip("Closes the project and shows the project hub");
                    ImGui::MenuItem("Project settings", nullptr, &workspace.projectSettingsWindow.show);
                    if (ImGui::MenuItem("Save project", nullptr, false, workspace.projectSettingsWindow.hasUnsavedChanges))
                        workspace.projectSettingsWindow.Save(Singletons::project);
                    ImGui::Separator();
                    ImGui::MenuItem("New scene...", nullptr, &newScene);
                    RenderRecentScenesMenu();
                    ImGui::MenuItem("Save scene", getShortcut("File: Save scene").c_str(), &saveScene, activeSceneWindow != nullptr);
                    ImGui::Separator();
                    ImGui::MenuItem("Exit", nullptr, &exitEditor);
                    /*
                    ImGui::Separator();
                    ImGui::MenuItem("Import files", nullptr, &importFiles);
                    */
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Edit")) {
                    auto history = workspace.GetActiveHistory();
                    auto undoName = history ? history->GetUndoName() : std::string();
                    auto redoName = history ? history->GetRedoName() : std::string();
                    if (ImGui::MenuItem(undoName.empty() ? "Undo" : ("Undo " + undoName).c_str(), getShortcut("Edit: Undo").c_str(), false, workspace.CanUndo()))
                        workspace.Undo();
                    if (ImGui::MenuItem(redoName.empty() ? "Redo" : ("Redo " + redoName).c_str(), getShortcut("Edit: Redo").c_str(), false, workspace.CanRedo()))
                        workspace.Redo();
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    ImGui::SeparatorText("Windows");
                    ImGui::MenuItem("Content browser", getShortcut("Window: Toggle content browser").c_str(), &workspace.contentBrowserWindow.show);
                    ImGui::MenuItem("Log", getShortcut("Window: Toggle log").c_str(), &workspace.logWindow.show);
                    ImGui::MenuItem("Profiler", nullptr, &workspace.profilerWindow.show);
                    ImGui::MenuItem("Geometry brush", nullptr, &workspace.geometryBrushWindow.show);

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
                    if (ImGui::MenuItem("Command palette", getShortcut("Help: Command palette").c_str()))
                        Singletons::commandPalette->Open();
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
            for (auto& sceneWindow : workspace.sceneWindows) {
                workspace.DockAsTab(*sceneWindow);
                sceneWindow->Render();
            }

            sectionScope.Next("Asset editor windows");
            for (auto& codeEditorWindow : workspace.codeEditorWindows) {
                workspace.DockAsTab(*codeEditorWindow);
                codeEditorWindow->Render();
            }
            for (auto& codeEditorWindow : workspace.codeEditorWindows) {
                if (!codeEditorWindow->show && codeEditorWindow->hasUnsavedChanges) {
                    codeEditorWindow->show = true;
                    AddUnsavedWindow(codeEditorWindow);
                }
                if (codeEditorWindow->inFocus) {
                    workspace.activeCodeEditorWindow = codeEditorWindow;
                    workspace.activeAssetEditorWindow = nullptr;
                }
            }
            std::erase_if(workspace.codeEditorWindows, [](const auto& window) { return !window->show; });
            if (workspace.activeCodeEditorWindow && !workspace.activeCodeEditorWindow->show)
                workspace.activeCodeEditorWindow = nullptr;

            for (auto& assetEditorWindow : workspace.assetEditorWindows) {
                workspace.DockAsTab(*assetEditorWindow);
                assetEditorWindow->Render();
            }

            // This does active changes to the scene, render after scene windows
            sectionScope.Next("Geometry brush window");
            workspace.DockAsTab(workspace.geometryBrushWindow);
            workspace.geometryBrushWindow.Render(activeSceneWindow);

            workspace.UpdateDrawers();

            sectionScope.Next("Content browser");
            workspace.DockPinnedDrawer(workspace.contentBrowserWindow);
            workspace.contentBrowserWindow.Render();
            sectionScope.Next("Log window");
            workspace.DockPinnedDrawer(workspace.logWindow);
            workspace.logWindow.Render();
            sectionScope.Next("Profiler window");
            workspace.DockPinnedDrawer(workspace.profilerWindow);
            workspace.profilerWindow.Render();
            sectionScope.Next("Settings window");
            sectionScope.Next("Project settings window");
            workspace.DockAsTab(workspace.projectSettingsWindow);
            workspace.projectSettingsWindow.Render(Singletons::project);

            ImGui::End();
        }
        else {
            sectionScope.Next("Maximized scene");
            RenderSceneMaximized();
        }
        sectionScope.Next("Popups and notifications");

        if (!playingMaximized)
            editorCommands.RenderShortcutsWindow(&showShortcutsWindow);

        Notifications::Display();
        UI::PopupPanels::Render();
        if (auto created = projectHub->RenderNewProjectPopup(window.GetSDLWindow()))
            OpenProject(*created);
        RenderUnsavedChangesPopup();
        Singletons::commandPalette->Render();

        // Generates the previews which everything rendered in this frame requested
        sectionScope.Next("Content previews");
        Singletons::contentPreviews->Update();

        // Inactive viewports can still have scene jobs that enqueue GPU work, scenes which weren't rendered drop their
        // render lists
        sectionScope.Next("Wait for scene updates");
        forEachScene([](Scene::Scene& scene) { scene.WaitForAsyncWorkCompletion(); });
        sectionScope.Next("ImGui render");
        ImGui::Render();
        Singletons::imguiWrapper->Render(true);

        if (bridge)
            bridge->AfterRender();

    }

    void App::RenderSceneMaximized() {

        auto activeSceneWindow = workspace.sceneWindows.empty() ? nullptr : workspace.sceneWindows[workspace.activeSceneIdx];

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("RenderWindow", nullptr, window_flags);

        activeSceneWindow->viewportPanel.RenderImage(activeSceneWindow->scene.Get(), true);

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
            auto heading = "Loading " + std::to_string(waitToLoadScenes.size()) + " scenes...";
            if (waitToLoadScenes.size() <= 3) {
                std::string names;
                for (const auto& handle : waitToLoadScenes)
                    names += (names.empty() ? "" : ", ") + Common::Path::GetFileNameWithoutExtension(handle.GetResource()->path);
                heading = "Loading " + names + "...";
            }

            // The spinner is centered above the heading, the window sizes itself to the heading
            auto textWidth = ImGui::CalcTextSize(heading.c_str()).x * UI::Widgets::headingScale;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (textWidth - ImGui::GetTextLineHeight() * 2.0f) * 0.5f));
            UI::Widgets::Spinner(2.0f);
            UI::Widgets::Heading(heading.c_str());
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
            workspace.contentBrowserWindow.show = true;

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
        // Code is shown with the regular font if this one is missing
        Theme::codeFont = io.Fonts->AddFontFromFileTTF(Loader::AssetLoader::GetFullPath("font/robotomono.ttf").c_str(), rasterSize);
        if (Theme::codeFont)
            Theme::codeFont->Scale = 15.0f / rasterSize;
        Singletons::imguiWrapper->RecreateImGuiResources();

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

    bool App::AllowQuit() {

        if (quitConfirmed)
            return true;

        for (const auto& window : workspace.sceneWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);
        for (const auto& window : workspace.assetEditorWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);
        for (const auto& window : workspace.codeEditorWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);

        auto unsavedProject = Singletons::project && workspace.projectSettingsWindow.hasUnsavedChanges;
        quitAfterUnsavedChanges = !unsavedWindows.empty() || unsavedProject;
        return !quitAfterUnsavedChanges;

    }

    void App::AddUnsavedWindow(const Ref<UI::Window>& window) {

        if (std::find(unsavedWindows.begin(), unsavedWindows.end(), window) == unsavedWindows.end())
            unsavedWindows.push_back(window);

    }

    void App::RenderUnsavedChangesPopup() {

        // Windows can close in other ways in the meantime, e.g. after a save
        std::erase_if(unsavedWindows, [&](const auto& window) { return !window->hasUnsavedChanges || !workspace.IsWindowOpen(window); });
        // The project is only asked about when the editor closes
        auto unsavedProject = quitAfterUnsavedChanges && Singletons::project && workspace.projectSettingsWindow.hasUnsavedChanges;
        if (unsavedWindows.empty() && !unsavedProject) {
            if (quitAfterUnsavedChanges && nextProject) {
                quitAfterUnsavedChanges = false;
                projectSwitchRequested = true;
            }
            else if (quitAfterUnsavedChanges) {
                quitConfirmed = true;
                Exit();
            }
            return;
        }

        // A running save shows its own popup, both would take turns opening
        if (Singletons::blockingOperation->block)
            return;

        const char* popupName = "Unsaved changes";
        if (!ImGui::IsPopupOpen(popupName))
            ImGui::OpenPopup(popupName);

        // Laid out like the other dialogs: centered with a fixed width, the question first and the actions at the bottom
        auto viewport = ImGui::GetMainViewport();
        auto width = std::clamp(viewport->Size.x * 0.3f, 420.0f, 560.0f);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
        auto open = ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
        ImGui::PopStyleVar();
        if (!open)
            return;

        // The scenes, assets, code and the project with their files
        std::vector<std::pair<std::string, std::string>> unsaved;
        for (const auto& window : unsavedWindows)
            unsaved.push_back(workspace.GetWindowFile(window));
        if (unsavedProject)
            unsaved.emplace_back("the project " + Singletons::project->name, Singletons::project->path);
        auto files = std::to_string(unsavedWindows.size()) + (unsavedWindows.size() == 1 ? " file" : " files");
        auto heading = unsaved.size() == 1 ? "Save the changes to " + unsaved.front().first + "?" :
            "Save the changes to " + files + (unsavedProject ? " and the project?" : "?");
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextWrapped("%s", heading.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        auto consequence = !quitAfterUnsavedChanges ? "The window closes afterwards." :
            nextProject ? "The editor opens the other project afterwards." : "The editor closes afterwards.";
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted());
        ImGui::TextWrapped("%s Changes which aren't saved are lost.", consequence);
        ImGui::PopStyleColor();

        // A single one is named in the question already
        if (unsaved.size() > 1) {
            ImGui::Spacing();
            for (const auto& [label, file] : unsaved) {
                ImGui::Bullet();
                ImGui::TextUnformatted(label.c_str());
                ImGui::SameLine();
                ImGui::TextColored(Theme::TextMuted(), "%s", file.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // The destructive action stands apart on the left, saving is the rightmost and the default with Enter
        auto& style = ImGui::GetStyle();
        auto buttonWidth = std::max({ ImGui::CalcTextSize("Save").x, ImGui::CalcTextSize("Cancel").x,
            ImGui::CalcTextSize("Don't save").x }) + 4.0f * style.FramePadding.x;
        Theme::PushDangerButton();
        bool discard = ImGui::Button("Don't save", ImVec2(buttonWidth, 0.0f));
        Theme::PopDangerButton();
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 2.0f * buttonWidth - style.ItemSpacing.x);
        bool cancel = ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        ImGui::SameLine();
        Theme::PushPrimaryButton();
        bool save = ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        Theme::PopPrimaryButton();

        if (save || discard) {
            for (const auto& window : unsavedWindows) {
                if (save)
                    workspace.SaveWindow(window);
                // Saves of scenes run as blocking operations, which finish before the window is deleted. Quitting
                // keeps the windows, such that they open again with the next start
                window->hasUnsavedChanges = false;
                if (!quitAfterUnsavedChanges)
                    window->show = false;
            }
        }

        if (unsavedProject && save)
            workspace.projectSettingsWindow.Save(Singletons::project);
        // The changes of a discarded project are lost with the editor
        if (unsavedProject && discard)
            workspace.projectSettingsWindow.hasUnsavedChanges = false;

        if (save || discard || cancel) {
            unsavedWindows.clear();
            // A cancelled quit doesn't switch the project either
            if (cancel) {
                quitAfterUnsavedChanges = false;
                nextProject.reset();
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

    }

    void App::SetupMainDockspace(ImGuiID dsID) {

        ImGui::SetWindowSize(ImVec2(370.0f, 680.0f));
        ImGui::SetWindowPos(ImVec2(640.0f, 55.0f));

        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dsID, ImGui::GetMainViewport()->WorkSize);

        for (auto& sceneWindow : workspace.sceneWindows) {
            ImGui::DockBuilderDockWindow(sceneWindow->GetNameID(), dsID);
        }

        ImGui::DockBuilderFinish(dsID);

        workspace.projectSettingsWindow.show = false;
        // Pinned drawers keep their place in the new layout
        for (auto& drawer : workspace.drawers)
            drawer.window->show = drawer.window->requestDocking = drawer.window->pinned;

        for (const auto& sceneWindow : workspace.sceneWindows) {
            sceneWindow->resetDockingLayout = true;
            sceneWindow->show = true;
        }

        resetDockspaceLayout = false;
        dockLoadingScenes = true;

    }

    void App::SubscribeToResourceEvents() {

        sceneCreateSubscription = ResourceManager<Scene::Scene>::Subscribe(ResourceTopic::ResourceCreate,
            [&](Ref<Resource<Scene::Scene>>& scene) {
                waitToLoadScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                if (!restoringScenes)
                    userOpenedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->openedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->AddRecentScene(scene->path);
            });

        workspace.contentBrowserWindow.onRename = [this](const std::string& oldPath, const std::string& newPath, bool updateReferences) {
            workspace.RenamePaths(oldPath, newPath, updateReferences);
            };

        // Also kind of a resource event
        dropSubscription = Events::EventManager::DropEventDelegate.Subscribe([&](Events::DropEvent event) {
            if (!event.file.empty()) {
                workspace.contentBrowserWindow.show = true;
                auto destinationDirectory = Common::Path::GetAbsolute(workspace.contentBrowserWindow.currentDirectory);

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
