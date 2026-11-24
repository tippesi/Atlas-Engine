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

        contentBrowserWindow.currentDirectory = Loader::AssetLoader::GetDataDirectory();
        Serialization::DeserializeContentBrowserWindow(contentBrowserWindow);

        Serialization::DeserializeDrawerWindow(logWindow);
        Serialization::DeserializeDrawerWindow(profilerWindow);
        restoringScenes = false;

        // A hand edited config may name a drawer twice or one which doesn't exist
        auto& statusBarDrawers = Singletons::config->statusBarDrawers;
        std::vector<std::string> knownDrawers;
        for (const auto& name : statusBarDrawers)
            if (FindDrawer(name) && std::find(knownDrawers.begin(), knownDrawers.end(), name) == knownDrawers.end())
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

        RegisterCommands();

        projectHub = std::make_unique<UI::ProjectHubWindow>(engineDataDirectory,
            (std::filesystem::path(engineDataDirectory).parent_path() / "templates").generic_string());

        if (Singletons::project) {
            projectSettingsWindow.onChange = [this](const Project&) { ApplyProjectSettings(); };
            ApplyProjectSettings();
            OpenStartScene();
            // --mcp-port <port> if another program uses the default port
            auto port = Bridge::McpServer::defaultPort;
            for (size_t i = 0; i + 1 < args.size(); i++) {
                if (args[i] == "--mcp-port")
                    port = std::stoi(args[i + 1]);
            }
            bridge = std::make_unique<EditorBridge>(Singletons::project, port, [this]() { return GetActiveSceneWindow(); });
            bridge->addEditorFunctions = [this](Scripting::Binder& editor) { AddEditorFunctions(editor); };
            // Recorded for the bindings and completions before the first script runs
            sol::state recorder;
            Scripting::Binder editorTable(recorder.create_table(), "Editor");
            AddEditorFunctions(editorTable);
            // Lua of the agent changes the project like the settings window: applied, undoable there and saved by the user
            bridge->onProjectChange = [this](const Project& before) {
                projectSettingsWindow.RecordChange(Singletons::project, before, "project (Lua)");
                };
        }

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

        for (const auto& sceneWindow : sceneWindows) {
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

    void App::AddEditorFunctions(Scripting::Binder& editor) {

        // { name, kind, path, shown, focused, unsaved } of the tabs and drawers
        editor.set_function("Windows", [this](sol::this_state state) {
            sol::state_view lua(state);
            sol::table windows = lua.create_table();
            auto add = [&](const UI::Window& window, const char* kind, const std::string& path) {
                windows.add(lua.create_table_with("name", window.name, "kind", kind, "path", path, "shown", window.show,
                    "focused", window.inFocus, "unsaved", window.hasUnsavedChanges));
                };
            for (const auto& window : sceneWindows)
                add(*window, "scene", UI::SceneWindow::GetScenePath(window->scene));
            for (const auto& window : assetEditorWindows)
                add(*window, "asset", window->GetAssetPath());
            for (const auto& window : codeEditorWindows)
                add(*window, "code", window->GetPath());
            add(projectSettingsWindow, "project settings", Singletons::project ? Singletons::project->path : "");
            add(contentBrowserWindow, "content browser", contentBrowserWindow.currentDirectory);
            add(logWindow, "log", "");
            add(profilerWindow, "profiler", "");
            return windows;
            });
        // Opens a file like a double click in the content browser: scenes, assets, scripts and shaders
        editor.set_function("Open", [this](const std::string& path) {
            if (!std::filesystem::exists(Loader::AssetLoader::GetFullPath(path)))
                throw std::runtime_error(path + " doesn't exist in the asset directory");
            auto error = OpenInAssetEditor(path);
            if (!error.empty())
                throw std::runtime_error(error);
            });
        // Shows a window and gives it the focus, found by its name or path
        editor.set_function("Focus", [this](const std::string& nameOrPath) {
            std::vector<std::pair<UI::Window*, std::string>> windows;
            for (const auto& window : sceneWindows)
                windows.emplace_back(window.get(), UI::SceneWindow::GetScenePath(window->scene));
            for (const auto& window : assetEditorWindows)
                windows.emplace_back(window.get(), window->GetAssetPath());
            for (const auto& window : codeEditorWindows)
                windows.emplace_back(window.get(), window->GetPath());
            windows.emplace_back(&projectSettingsWindow, "");
            windows.emplace_back(&contentBrowserWindow, "");
            windows.emplace_back(&logWindow, "");
            windows.emplace_back(&profilerWindow, "");
            for (auto& [window, path] : windows) {
                if (window->name != nameOrPath && path != nameOrPath)
                    continue;
                window->show = true;
                window->requestFocus = true;
                return;
            }
            throw std::runtime_error("There is no window " + nameOrPath + ", Editor.Windows() lists them");
            });
        // The text of an open code window, also while it isn't saved
        editor.set_function("GetText", [this](const std::string& path) -> std::string {
            for (const auto& window : codeEditorWindows)
                if (window->GetPath() == path)
                    return window->GetText();
            throw std::runtime_error(path + " isn't open in a code window, Editor.Open(path) opens it");
            });
        // Replaces the text of a code window, opened if needed, as one undo step there. The user saves it
        editor.set_function("SetText", [this](const std::string& path, const std::string& text) {
            auto find = [&]() {
                return std::find_if(codeEditorWindows.begin(), codeEditorWindows.end(),
                    [&](const auto& window) { return window->GetPath() == path; });
                };
            if (find() == codeEditorWindows.end())
                OpenInAssetEditor(path);
            auto window = find();
            if (window == codeEditorWindows.end())
                throw std::runtime_error(path + " can't be opened in a code window");
            (*window)->SetText(text);
            });
        // Like the play and stop buttons of the active scene window, they take effect in the next frame
        editor.set_function("Play", [this]() {
            auto sceneWindow = GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->scene.IsLoaded())
                throw std::runtime_error("There is no loaded scene window, Editor.Focus(path) activates one");
            if (sceneWindow->isPlaying)
                throw std::runtime_error(sceneWindow->name + " is already playing");
            if (!sceneWindow->StartPlaying())
                throw std::runtime_error(sceneWindow->name + " can't play, it has no main camera or another operation is running");
            });
        editor.set_function("Stop", [this]() {
            auto sceneWindow = GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->isPlaying)
                throw std::runtime_error("The active scene window isn't playing");
            sceneWindow->StopPlaying();
            });

    }

    void App::SwitchProject() {

        WaitForRender();
        auto path = *nextProject;
        UnloadContent();

        sceneWindows.clear();
        assetEditorWindows.clear();
        codeEditorWindows.clear();
        activeAssetEditorWindow.reset();
        waitToLoadScenes.clear();
        userOpenedScenes.clear();
        unsavedWindows.clear();
        activeCodeEditorWindow.reset();
        emptyStateScenes.clear();
        quitAfterUnsavedChanges = false;
        quitConfirmed = false;
        activeSceneIdx = 0;
        drawerDockNodeID = 0;
        systemUiScale = 0.0f;
        fontRasterSize = 0.0f;
        emptyStateVisible = false;
        resetDockspaceLayout = false;
        dockLoadingScenes = false;
        restoringScenes = true;
        playingMaximized = false;
        projectHub.reset();
        projectSettingsWindow.history.Clear();
        projectSettingsWindow.hasUnsavedChanges = false;
        projectSettingsWindow.show = false;
        commandPalette = UI::CommandPalette();
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

        for (const auto& sceneWindow : sceneWindows) {
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            // There is no update anymore which would run the queued operations
            Singletons::blockingOperation->Finish();
            if (!sceneWindow->IsPrefab())
                Serialization::SerializeSceneWindow(sceneWindow);
        }

        Serialization::SerializeContentBrowserWindow(contentBrowserWindow);
        Serialization::SerializeDrawerWindow(logWindow);
        Serialization::SerializeDrawerWindow(profilerWindow);
        if (configHitchSettings)
            HitchRecorder::SetSettings(*configHitchSettings);
        Serialization::SerializeConfig();
        ContentDiscovery::Shutdown();

        bridge.reset();
        Singletons::Destruct();

        ResourceManager<Scene::Scene>::Unsubscribe(ResourceTopic::ResourceCreate, sceneCreateSubscription);
        Events::EventManager::DropEventDelegate.Unsubscribe(dropSubscription);

        ImGui::DestroyContext();

    }

    void App::Update(float deltaTime) {

        if (projectSwitchRequested)
            SwitchProject();

        const ImGuiIO& io = ImGui::GetIO();

        CPUProfiler::Scope sectionScope("Content discovery");
        // Will lead to frame time spikes, don't do it if content can't be seen
        if (!playingMaximized)
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

        Loader::ModelImporter::ApplyBackgroundImports();

        sectionScope.Next("Content previews update");
        // Also while no previews are shown, they release what they don't need anymore
        Singletons::contentPreviews->ReleaseUnused();

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

        // Windows with unsaved changes stay open until the user decided about them
        for (const auto& assetEditorWindow : assetEditorWindows) {
            if (!assetEditorWindow->show && assetEditorWindow->hasUnsavedChanges) {
                assetEditorWindow->show = true;
                AddUnsavedWindow(assetEditorWindow);
            }
        }
        std::erase_if(assetEditorWindows, [](const auto& window) { return !window->show; });
        if (activeAssetEditorWindow && !activeAssetEditorWindow->show)
            activeAssetEditorWindow = nullptr;
        for (const auto& assetEditorWindow : assetEditorWindows)
            if (assetEditorWindow->inFocus) {
                activeAssetEditorWindow = assetEditorWindow;
                activeCodeEditorWindow = nullptr;
            }
        // Drags outside of viewports, e.g. on a slider, must not turn the next camera that gets the focus
        auto mouseMovedCamera = false;
        for (const auto& assetEditorWindow : assetEditorWindows) {
            if (!assetEditorWindow->viewportPanel.isFocused || !assetEditorWindow->cameraEntity.IsValid() || ImGuizmo::IsUsing())
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

                // Projects saved before they had the scene settings take them from their first scene
                auto& project = Singletons::project;
                if (project->AdoptSceneSettings(*waitToLoadScenes.back().Get()))
                    project->Save();
                project->ApplyTo(*waitToLoadScenes.back().Get());

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
            ImGuizmo::Enable(true);
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
                activeSceneIdx = sceneCounter;
                activeAssetEditorWindow = nullptr;
                activeCodeEditorWindow = nullptr;
            }

            sceneCounter++;
        }

        // Reverse such that indices remain valid
        std::reverse(toBeDeletedSceneWindows.begin(), toBeDeletedSceneWindows.end());
        for (auto sceneWindowIdx : toBeDeletedSceneWindows) {
            // Serialize window before erasing it
            if (sceneWindows[sceneWindowIdx]->isPlaying)
                sceneWindows[sceneWindowIdx]->StopPlaying();
            // Queued operations reference the window
            Singletons::blockingOperation->Finish();
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

        // Asset editors have their own gizmos, e.g. for the pins of soft bodies
        ImGuizmo::Enable(activeAssetEditorWindow || activeSceneWindow->guizmo.needEnabled);

    }

    void App::Render(float deltaTime) {

        graphicsDevice->WaitForPreviousFrameSubmission();

        auto forEachScene = [&](const auto& function) {

            for (const auto& sceneWindow : sceneWindows) {
                if (sceneWindow->scene.IsLoaded())
                    function(*sceneWindow->scene.Get());
            }

            // The preview scenes of asset editors are updated even if their viewport isn't rendered
            for (const auto& assetEditorWindow : assetEditorWindows) {
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
                    if (ImGui::MenuItem("New project..."))
                        projectHub->OpenNewProjectPopup();
                    if (ImGui::MenuItem("Open project..."))
                        OpenProject("");
                    ImGui::SetItemTooltip("Closes the project and shows the project hub");
                    ImGui::MenuItem("Project settings", nullptr, &projectSettingsWindow.show);
                    if (ImGui::MenuItem("Save project", nullptr, false, projectSettingsWindow.hasUnsavedChanges))
                        projectSettingsWindow.Save(Singletons::project);
                    ImGui::Separator();
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
                DockAsTab(*sceneWindow);
                sceneWindow->Render();
            }

            sectionScope.Next("Asset editor windows");
            for (auto& codeEditorWindow : codeEditorWindows) {
                DockAsTab(*codeEditorWindow);
                codeEditorWindow->Render();
            }
            for (auto& codeEditorWindow : codeEditorWindows) {
                if (!codeEditorWindow->show && codeEditorWindow->hasUnsavedChanges) {
                    codeEditorWindow->show = true;
                    AddUnsavedWindow(codeEditorWindow);
                }
                if (codeEditorWindow->inFocus) {
                    activeCodeEditorWindow = codeEditorWindow;
                    activeAssetEditorWindow = nullptr;
                }
            }
            std::erase_if(codeEditorWindows, [](const auto& window) { return !window->show; });
            if (activeCodeEditorWindow && !activeCodeEditorWindow->show)
                activeCodeEditorWindow = nullptr;

            for (auto& assetEditorWindow : assetEditorWindows) {
                DockAsTab(*assetEditorWindow);
                assetEditorWindow->Render();
            }

            // This does active changes to the scene, render after scene windows
            sectionScope.Next("Geometry brush window");
            DockAsTab(geometryBrushWindow);
            geometryBrushWindow.Render(activeSceneWindow);

            UpdateDrawers();

            sectionScope.Next("Content browser");
            DockPinnedDrawer(contentBrowserWindow);
            contentBrowserWindow.Render();
            sectionScope.Next("Log window");
            DockPinnedDrawer(logWindow);
            logWindow.Render();
            sectionScope.Next("Profiler window");
            DockPinnedDrawer(profilerWindow);
            profilerWindow.Render();
            sectionScope.Next("Settings window");
            sectionScope.Next("Project settings window");
            DockAsTab(projectSettingsWindow);
            projectSettingsWindow.Render(Singletons::project);

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
        if (auto created = projectHub->RenderNewProjectPopup(window.GetSDLWindow()))
            OpenProject(*created);
        RenderUnsavedChangesPopup();
        commandPalette.Render();

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
        // Code is shown with the regular font if this one is missing
        Theme::codeFont = io.Fonts->AddFontFromFileTTF(Loader::AssetLoader::GetFullPath("font/robotomono.ttf").c_str(), rasterSize);
        if (Theme::codeFont)
            Theme::codeFont->Scale = 15.0f / rasterSize;
        Singletons::imguiWrapper->RecreateImGuiResources();

    }

    void App::Undo() {

        if (activeCodeEditorWindow) {
            activeCodeEditorWindow->Undo();
            return;
        }

        if (projectSettingsWindow.inFocus) {
            projectSettingsWindow.history.Undo();
            return;
        }

        if (activeAssetEditorWindow) {
            activeAssetEditorWindow->Undo();
            return;
        }

        auto sceneWindow = GetActiveSceneWindow();
        if (sceneWindow && !sceneWindow->isPlaying)
            sceneWindow->Undo();

    }

    void App::Redo() {

        if (activeCodeEditorWindow) {
            activeCodeEditorWindow->Redo();
            return;
        }

        if (projectSettingsWindow.inFocus) {
            projectSettingsWindow.history.Redo();
            return;
        }

        if (activeAssetEditorWindow) {
            activeAssetEditorWindow->Redo();
            return;
        }

        auto sceneWindow = GetActiveSceneWindow();
        if (sceneWindow && !sceneWindow->isPlaying)
            sceneWindow->Redo();

    }

    bool App::CanUndo() const {

        if (activeCodeEditorWindow)
            return activeCodeEditorWindow->CanUndo();
        auto history = GetActiveHistory();
        return history && history->CanUndo();

    }

    bool App::CanRedo() const {

        if (activeCodeEditorWindow)
            return activeCodeEditorWindow->CanRedo();
        auto history = GetActiveHistory();
        return history && history->CanRedo();

    }

    const UndoHistory* App::GetActiveHistory() const {

        if (projectSettingsWindow.inFocus)
            return &projectSettingsWindow.history;

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
        commandPalette.Register({ .category = "File", .name = "New project...",
            .action = [this]() { projectHub->OpenNewProjectPopup(); } });
        commandPalette.Register({ .category = "File", .name = "Open project...",
            .action = [this]() { OpenProject(""); } });
        commandPalette.Register({ .category = "File", .name = "Export Lua annotations",
            .action = [this]() { ExportLuaAnnotations(); } });
        commandPalette.Register({ .category = "File", .name = "New scene...",
            .action = []() { UI::PopupPanels::isNewScenePopupVisible = true; } });
        commandPalette.Register({ .category = "File", .name = "Save project",
            .action = [this]() { projectSettingsWindow.Save(Singletons::project); },
            .isEnabled = [this]() { return projectSettingsWindow.hasUnsavedChanges; } });
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
        registerPin("profiler", &profilerWindow);
        registerWindow("project settings", &projectSettingsWindow.show);
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
        commandPalette.Register({ .category = "Properties", .name = "Search entity properties", .shortcut = "Ctrl+F",
            .action = [this]() {
                auto& panel = GetActiveSceneWindow()->scenePropertiesPanel;
                panel.entityPropertiesPanel.focusSearch = true;
                ImGui::SetWindowFocus(panel.GetNameID());
                },
            .isEnabled = [this, hasScene]() { return hasScene() && GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntity.IsValid(); } });
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

        // Code window in front
        auto hasCodeWindow = [this]() { return activeCodeEditorWindow != nullptr; };
        commandPalette.Register({ .category = "Code", .name = "Save file", .shortcut = "Ctrl+S",
            .action = [this]() { activeCodeEditorWindow->Save(); }, .isEnabled = hasCodeWindow });
        commandPalette.Register({ .category = "Code", .name = "Find in file", .shortcut = "Ctrl+F",
            .action = [this]() { activeCodeEditorWindow->OpenSearch(false); }, .isEnabled = hasCodeWindow });
        commandPalette.Register({ .category = "Code", .name = "Replace in file", .shortcut = "Ctrl+H",
            .action = [this]() { activeCodeEditorWindow->OpenSearch(true); }, .isEnabled = hasCodeWindow });

        // Asset editor in front
        auto hasAssetEditor = [this]() { return activeAssetEditorWindow != nullptr; };
        commandPalette.Register({ .category = "Asset editor", .name = "Save asset", .shortcut = "Ctrl+S",
            .action = [this]() { activeAssetEditorWindow->Save(); activeAssetEditorWindow->hasUnsavedChanges = false; },
            .isEnabled = hasAssetEditor });
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

                auto warningCount = logWindow.GetUnseenWarningCount();
                auto errorCount = logWindow.GetUnseenErrorCount();

                // The drawers chosen for the status bar, a right click on it changes them
                auto contentBrowserHovered = false;
                auto firstDrawer = true;
                for (const auto& name : Singletons::config->statusBarDrawers) {
                    auto drawer = FindDrawer(name);
                    if (!drawer)
                        continue;
                    if (!firstDrawer)
                        gap(4.0f);
                    firstDrawer = false;

                    auto isLog = drawer->window == &logWindow;
                    auto hovered = RenderDrawerButton(*drawer->window, drawer->icon, drawer->name, drawer->shortcut,
                        isLog ? errorCount : 0, isLog ? warningCount : 0);
                    if (drawer->window == &contentBrowserWindow)
                        contentBrowserHovered = hovered;
                }

                // Dragging something onto the button opens the content browser as a drop target
                auto isDragging = ImGui::IsDragDropActive();
                drawerSpringTime = isDragging && contentBrowserHovered ? drawerSpringTime + ImGui::GetIO().DeltaTime : 0.0f;
                if (drawerSpringTime > 0.35f)
                    contentBrowserWindow.show = true;

                RenderStatusBarMenu();

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

                // Jobs come in bursts, the peak of the last half second avoids a flickering count. Short work isn't mentioned
                auto deltaTime = ImGui::GetIO().DeltaTime;
                auto backgroundJobs = JobSystem::GetPendingJobCount(JobPriority::Medium) +
                    JobSystem::GetPendingJobCount(JobPriority::Low);
                backgroundJobPeak = std::max(backgroundJobPeak, backgroundJobs);

                backgroundJobRefreshTime += deltaTime;
                if (backgroundJobRefreshTime > 0.5f || backgroundJobShown == 0) {
                    backgroundJobShown = backgroundJobPeak;
                    backgroundJobPeak = backgroundJobs;
                    backgroundJobRefreshTime = 0.0f;
                }
                backgroundJobTime = backgroundJobShown > 0 ? backgroundJobTime + deltaTime : 0.0f;

                if (backgroundJobTime > 0.5f && backgroundJobShown > 0) {
                    gap(16.0f);
                    const char* dots[] = { ".", "..", "..." };
                    char jobText[64];
                    std::snprintf(jobText, sizeof(jobText), "%d background %s%s", backgroundJobShown, backgroundJobShown == 1 ? "job" : "jobs",
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
        else if (*shortcut)
            ImGui::SetItemTooltip("Toggle %s (%s)", label, shortcut);
        else
            ImGui::SetItemTooltip("Toggle %s", label);

        return hovered;

    }

    void App::UpdateDrawers() {

        // Only one drawer is open at a time, the last one opened replaces the other one
        for (auto& opening : drawers) {
            if (!opening.window->IsOpening())
                continue;
            for (auto& other : drawers) {
                if (other.window != opening.window && other.window->show && !other.window->pinned)
                    opening.window->TakeOver(*other.window);
            }
            break;
        }

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
        for (auto& drawer : drawers)
            requestDockingIfLost(*drawer.window);

        drawerDockNodeID = 0;
        if (std::none_of(drawers.begin(), drawers.end(), [](const Drawer& drawer) { return drawer.window->requestDocking; }))
            return;

        // A drawer that is docked already shares its node with the new one
        auto getDockID = [](const UI::DrawerWindow& drawer) -> ImGuiID {
            auto window = ImGui::FindWindowByName(drawer.GetNameID());
            return drawer.pinned && !drawer.requestDocking && window ? window->DockId : 0;
            };
        for (const auto& drawer : drawers) {
            drawerDockNodeID = getDockID(*drawer.window);
            if (drawerDockNodeID)
                return;
        }

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

    void App::RenderStatusBarMenu() {

        if (!ImGui::BeginPopupContextWindow("Status bar menu"))
            return;

        auto& shownDrawers = Singletons::config->statusBarDrawers;

        ImGui::TextDisabled("Drawers in the status bar, drag to reorder");
        ImGui::Separator();

        int32_t removedDrawer = -1, draggedDrawer = -1;
        std::vector<float> rowTops;
        for (int32_t i = 0; i < int32_t(shownDrawers.size()); i++) {
            // The id follows the entry, such that it stays active while it moves past others
            ImGui::PushID(shownDrawers[i].c_str());
            auto shown = true;
            if (ImGui::Checkbox("##Shown", &shown))
                removedDrawer = i;
            ImGui::SameLine();
            ImGui::Selectable(shownDrawers[i].c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups);
            rowTops.push_back(ImGui::GetItemRectMin().y);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                draggedDrawer = i;
            ImGui::PopID();
        }
        // A dragged entry moves to the row below the mouse
        if (draggedDrawer >= 0 && removedDrawer < 0) {
            int32_t targetDrawer = 0;
            for (int32_t i = 1; i < int32_t(rowTops.size()); i++)
                if (ImGui::GetIO().MousePos.y >= rowTops[i])
                    targetDrawer = i;
            if (targetDrawer != draggedDrawer) {
                auto name = shownDrawers[draggedDrawer];
                shownDrawers.erase(shownDrawers.begin() + draggedDrawer);
                shownDrawers.insert(shownDrawers.begin() + targetDrawer, name);
            }
        }
        if (removedDrawer >= 0)
            shownDrawers.erase(shownDrawers.begin() + removedDrawer);

        // Drawers which aren't shown are added at the end
        for (const auto& drawer : drawers) {
            if (std::find(shownDrawers.begin(), shownDrawers.end(), drawer.name) != shownDrawers.end())
                continue;
            ImGui::PushID(drawer.name);
            auto shown = false;
            if (ImGui::Checkbox("##Shown", &shown))
                shownDrawers.push_back(drawer.name);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", drawer.name);
            ImGui::PopID();
        }

        ImGui::EndPopup();

    }

    App::Drawer* App::FindDrawer(const std::string& name) {

        auto iter = std::find_if(drawers.begin(), drawers.end(), [&](const Drawer& drawer) { return name == drawer.name; });
        return iter != drawers.end() ? &*iter : nullptr;

    }

    bool App::AllowQuit() {

        if (quitConfirmed)
            return true;

        for (const auto& window : sceneWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);
        for (const auto& window : assetEditorWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);
        for (const auto& window : codeEditorWindows)
            if (window->hasUnsavedChanges)
                AddUnsavedWindow(window);

        auto unsavedProject = Singletons::project && projectSettingsWindow.hasUnsavedChanges;
        quitAfterUnsavedChanges = !unsavedWindows.empty() || unsavedProject;
        return !quitAfterUnsavedChanges;

    }

    void App::AddUnsavedWindow(const Ref<UI::Window>& window) {

        if (std::find(unsavedWindows.begin(), unsavedWindows.end(), window) == unsavedWindows.end())
            unsavedWindows.push_back(window);

    }

    bool App::IsWindowOpen(const Ref<UI::Window>& window) const {

        auto contains = [&](const auto& windows) {
            return std::any_of(windows.begin(), windows.end(), [&](const auto& open) { return open == window; });
            };
        return contains(sceneWindows) || contains(assetEditorWindows) || contains(codeEditorWindows);

    }

    void App::SaveWindow(const Ref<UI::Window>& window) {

        if (auto sceneWindow = std::dynamic_pointer_cast<UI::SceneWindow>(window)) {
            // The scene of a playing window is the state of the game
            if (sceneWindow->isPlaying)
                sceneWindow->StopPlaying();
            sceneWindow->SaveScene();
        }
        else if (auto assetEditorWindow = std::dynamic_pointer_cast<UI::AssetEditorWindow>(window)) {
            assetEditorWindow->Save();
        }
        else if (auto codeEditorWindow = std::dynamic_pointer_cast<UI::CodeEditorWindow>(window)) {
            codeEditorWindow->Save();
        }

    }

    std::pair<std::string, std::string> App::GetWindowFile(const Ref<UI::Window>& window) const {

        if (auto sceneWindow = std::dynamic_pointer_cast<UI::SceneWindow>(window)) {
            auto path = UI::SceneWindow::GetScenePath(sceneWindow->scene);
            return { Common::Path::GetFileNameWithoutExtension(path), path };
        }
        if (auto assetEditorWindow = std::dynamic_pointer_cast<UI::AssetEditorWindow>(window))
            return { window->name, assetEditorWindow->GetAssetPath() };
        if (auto codeEditorWindow = std::dynamic_pointer_cast<UI::CodeEditorWindow>(window))
            return { window->name, codeEditorWindow->GetPath() };
        return { window->name, "" };

    }

    void App::RenderUnsavedChangesPopup() {

        // Windows can close in other ways in the meantime, e.g. after a save
        std::erase_if(unsavedWindows, [&](const auto& window) { return !window->hasUnsavedChanges || !IsWindowOpen(window); });
        // The project is only asked about when the editor closes
        auto unsavedProject = quitAfterUnsavedChanges && Singletons::project && projectSettingsWindow.hasUnsavedChanges;
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
            unsaved.push_back(GetWindowFile(window));
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
                    SaveWindow(window);
                // Saves of scenes run as blocking operations, which finish before the window is deleted. Quitting
                // keeps the windows, such that they open again with the next start
                window->hasUnsavedChanges = false;
                if (!quitAfterUnsavedChanges)
                    window->show = false;
            }
        }

        if (unsavedProject && save)
            projectSettingsWindow.Save(Singletons::project);
        // The changes of a discarded project are lost with the editor
        if (unsavedProject && discard)
            projectSettingsWindow.hasUnsavedChanges = false;

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

        projectSettingsWindow.show = false;
        // Pinned drawers keep their place in the new layout
        for (auto& drawer : drawers)
            drawer.window->show = drawer.window->requestDocking = drawer.window->pinned;

        for (const auto& sceneWindow : sceneWindows) {
            sceneWindow->resetDockingLayout = true;
            sceneWindow->show = true;
        }

        resetDockspaceLayout = false;
        dockLoadingScenes = true;

    }

    std::string App::OpenInAssetEditor(const std::string& path) {

        auto noEditor = "There is no editor for " + Common::Path::GetFileName(path);
        auto mapping = Content::contentTypeMapping.find(Common::Path::GetFileType(path));
        if (mapping == Content::contentTypeMapping.end()) {
            Notifications::Push({ .message = noEditor, .type = NotificationType::Warning });
            return noEditor;
        }
        auto type = mapping->second;
        if (type == ContentType::Animation) {
            // Animations play in the mesh editor focused last
            Ref<UI::MeshEditorWindow> assetEditorWindow;
            for (const auto& window : assetEditorWindows) {
                auto candidate = std::dynamic_pointer_cast<UI::MeshEditorWindow>(window);
                if (candidate && (!assetEditorWindow || candidate->lastFocusTime > assetEditorWindow->lastFocusTime))
                    assetEditorWindow = candidate;
            }
            auto animation = FileImporter::ImportFile<Animation::Animation>(path);
            if (!assetEditorWindow || !assetEditorWindow->Play(animation)) {
                std::string error = "Open a skinned mesh to play the animation on";
                Notifications::Push({ .message = error, .type = NotificationType::Error });
                return error;
            }
            return {};
        }

        if (type == ContentType::Prefab) {
            for (const auto& sceneWindow : sceneWindows) {
                if (sceneWindow->prefabPath == path)
                    return {};
            }

            auto scene = DataCreator::CreateScene(Common::Path::GetFileName(path), vec3(-2048.0f), vec3(2048.0f), 8);
            Singletons::project->ApplyTo(*scene);
            auto sceneWindow = CreateRef<UI::SceneWindow>(ResourceHandle<Scene::Scene>(scene), true);
            sceneWindow->OpenPrefab(path);
            OpenAsTab(*sceneWindow);
            sceneWindows.push_back(sceneWindow);
            return {};
        }

        if (type == ContentType::Script || type == ContentType::Shader) {
            auto opened = std::find_if(codeEditorWindows.begin(), codeEditorWindows.end(),
                [&](const auto& window) { return window->GetPath() == path; });
            if (opened != codeEditorWindows.end()) {
                (*opened)->requestFocus = true;
                return {};
            }
            codeEditorWindows.push_back(CreateRef<UI::CodeEditorWindow>(path));
            OpenAsTab(*codeEditorWindows.back());
            return {};
        }

        // Scenes open as a scene tab, like a double click in the content browser
        if (type == ContentType::Scene) {
            FileImporter::ImportFile<Scene::Scene>(path);
            return {};
        }

        if (type != ContentType::Material && type != ContentType::MaterialGraph && type != ContentType::Mesh) {
            Notifications::Push({ .message = noEditor, .type = NotificationType::Warning });
            return noEditor;
        }

        auto iter = std::find_if(assetEditorWindows.begin(), assetEditorWindows.end(),
            [&](const auto& window) { return window->GetAssetPath() == path; });
        if (iter != assetEditorWindows.end()) {
            (*iter)->requestFocus = true;
            activeAssetEditorWindow = *iter;
            return {};
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
        return {};

    }

    void App::DockAsTab(UI::Window& window) {

        if (window.show && !window.wasShown) {
            // The first frame shows the windows of the saved layout, they are docked there already or floating
            auto restored = ImGui::GetFrameCount() <= 1;
            auto imguiWindow = ImGui::FindWindowByName(window.GetNameID());
            auto settings = ImGui::FindWindowSettingsByID(ImHashStr(window.GetNameID()));
            auto dockId = imguiWindow ? imguiWindow->DockId : settings ? settings->DockId : 0;
            if (!restored || !dockId) {
                window.requestDocking = true;
                if (!window.dockNodeID)
                    window.dockNodeID = GetActiveDockNodeID();
            }
            if (!restored)
                window.requestFocus = true;
        }
        window.wasShown = window.show;
        DockNextToScenes(window);

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

        sceneCreateSubscription = ResourceManager<Scene::Scene>::Subscribe(ResourceTopic::ResourceCreate,
            [&](Ref<Resource<Scene::Scene>>& scene) {
                waitToLoadScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                if (!restoringScenes)
                    userOpenedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->openedScenes.push_back(ResourceHandle<Scene::Scene>(scene));
                Singletons::config->AddRecentScene(scene->path);
            });

        // Also kind of a resource event
        dropSubscription = Events::EventManager::DropEventDelegate.Subscribe([&](Events::DropEvent event) {
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
