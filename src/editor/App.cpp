#include "App.h"
#include "FileImporter.h"
#include "Singletons.h"
#include "Serializer.h"
#include "ui/panels/PopupPanels.h"
#include <ImGuizmo.h>

#include <chrono>
#include <thread>

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
    .assetDirectory = "../../data",
    .shaderDirectory = "shader"
};

namespace Atlas::Editor {

    void App::LoadContent() {

        auto icon = Atlas::Texture::Texture2D("icon.png");
        window.SetIcon(&icon);

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Add font with enlarged size and scale it down again
        // This means we can use scaled text up to 2x the size
        io.Fonts->AddFontFromFileTTF(
            Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(),
            32.0f
        )->Scale = 0.5f;

        Singletons::imguiWrapper = CreateRef<ImguiExtension::ImguiWrapper>();
        Singletons::imguiWrapper->Load(&window);
        Singletons::renderTarget = CreateRef<Renderer::RenderTarget>(1280, 720);
        Singletons::pathTraceRenderTarget = CreateRef<Renderer::PathTracerRenderTarget>(1280, 720);
        Singletons::mainRenderer = mainRenderer;
        Singletons::icons = CreateRef<Icons>();
        Singletons::config = CreateRef<Config>();

        mouseHandler = Input::MouseHandler(1.5f, 8.0f);
        keyboardHandler = Input::KeyboardHandler(7.0f, 5.0f);

        SubscribeToResourceEvents();

        Serializer::DeserializeConfig();

    }

    void App::UnloadContent() {

        Singletons::imguiWrapper->Unload();

        Serializer::SerializeConfig();

        Singletons::Destruct();

    }

    void App::Update(float deltaTime) {

        const ImGuiIO &io = ImGui::GetIO();

        Singletons::imguiWrapper->Update(&window, deltaTime);

        if (sceneWindows.empty())
            return;

        auto& config = Singletons::config;

        std::vector<size_t> toBeDeletedSceneWindows;

        // Find active scene and old scene windows
        size_t sceneCounter = 0;
        for (auto& sceneWindow : sceneWindows) {

            if (std::find(config->openedScenes.begin(), config->openedScenes.end(),
                sceneWindow.scene) == config->openedScenes.end()) {
                toBeDeletedSceneWindows.push_back(sceneCounter++);
                continue;
            }

            if (sceneWindow.viewportPanel.isFocused)
                activeSceneIdx = sceneCounter;

            sceneCounter++;
            
            sceneWindow.Update(deltaTime);
        }

        // Reverse such that indices remain valid
        std::reverse(toBeDeletedSceneWindows.begin(), toBeDeletedSceneWindows.end());
        for (auto sceneWindowIdx : toBeDeletedSceneWindows) {
            sceneWindows.erase(sceneWindows.begin() + sceneWindowIdx);
        }

        // Update active scene with input
        activeSceneIdx = std::min(activeSceneIdx, sceneWindows.size() - 1);

        auto& activeSceneWindow = sceneWindows[activeSceneIdx];

        auto cameraEntity = activeSceneWindow.cameraEntity;
        if (cameraEntity.IsValid() && activeSceneWindow.viewportPanel.isFocused &&
            !ImGuizmo::IsUsing() && !activeSceneWindow.isPlaying) {
            auto& camera = cameraEntity.GetComponent<CameraComponent>();
            mouseHandler.Update(camera, deltaTime);
            keyboardHandler.Update(camera, deltaTime);
        }

        if (activeSceneWindow.isPlaying) {
            auto playerSubset = activeSceneWindow.scene->GetSubset<PlayerComponent>();
            for (auto entity : playerSubset) {
                auto camera = entity.TryGetComponent<CameraComponent>();
                if (!camera)
                    continue;

                auto player = entity.GetComponent<PlayerComponent>();

                mouseHandler.Update(*camera, deltaTime);
                keyboardHandler.Update(*camera, player, deltaTime);
            }
        }

        // Update all scenes after input was applied
        for (auto& sceneWindow : sceneWindows) {

            if (sceneWindow.viewportPanel.isFocused)
                activeSceneIdx = sceneCounter;

            sceneCounter++;

            sceneWindow.Update(deltaTime);
        }

        ImGuizmo::Enable(activeSceneWindow.needGuizmoEnabled);

        // Launch BVH builds asynchronously
        auto buildRTStructure = [&]() {
            auto sceneMeshes = ResourceManager<Mesh::Mesh>::GetResources();

            for (const auto& mesh : sceneMeshes) {
                if (!mesh.IsLoaded())
                    continue;
                if (mesh->IsBVHBuilt())
                    continue;
                mesh->BuildBVH();
            }
            };

        if (!bvhBuilderFuture.valid()) {
            bvhBuilderFuture = std::async(std::launch::async, buildRTStructure);
            return;
        }
        else if(bvhBuilderFuture.wait_for(std::chrono::microseconds(0)) == std::future_status::ready) {
            bvhBuilderFuture.get();
        }

    }

    void App::Render(float deltaTime) {

        auto windowFlags = window.GetFlags();
        if (windowFlags & AE_WINDOW_HIDDEN || windowFlags & AE_WINDOW_MINIMIZED || !(windowFlags & AE_WINDOW_SHOWN)) {
            return;
        }

        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // ImGui::ShowDemoWindow();

        ImGuiViewport *viewport = ImGui::GetMainViewport();
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

        if (!windowsToAddToNode.empty()) {
            for (auto windowID : windowsToAddToNode)
                ImGui::DockBuilderDockWindow(windowID.c_str(), upperDockNodeID);

            windowsToAddToNode.clear();
        }

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(mainDsId, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (ImGui::BeginMainMenuBar()) {
            static bool openProject = false, saveProject = false, newScene = false, importFiles = false;
            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Open project", nullptr, &openProject);
                ImGui::MenuItem("Save project", nullptr, &saveProject);
                ImGui::Separator();
                ImGui::MenuItem("New scene", nullptr, &newScene);
                ImGui::Separator();
                ImGui::MenuItem("Import files", nullptr, &importFiles);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Reset layout", nullptr, &resetDockspaceLayout);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Renderer")) {
                auto& config = Singletons::config;
                ImGui::MenuItem("Pathtracer", nullptr, &config->pathTrace);
                ImGui::EndMenu();
            }

            if (newScene) {
                UI::PopupPanels::isNewScenePopupVisible = true;
                newScene = false;
            }

            ImGui::EndMainMenuBar();
        }

        UI::PopupPanels::Render();

        for (auto& sceneWindow : sceneWindows) {
            sceneWindow.Render();
        }

        contentBrowserWindow.Render();
        logWindow.Render();

        ImGui::End();

        ImGui::Render();
        Singletons::imguiWrapper->Render(true);

    }

    void App::SetupMainDockspace(ImGuiID dsID) {

        ImGui::SetWindowSize(ImVec2(370.0f, 680.0f));
        ImGui::SetWindowPos(ImVec2(640.0f, 55.0f));

        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID);

        ImGuiID dsIdUp, dsIdDown;
        ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Up, 0.8f, &dsIdUp, &dsIdDown);

        for (auto& sceneWindow : sceneWindows) {
            ImGui::DockBuilderDockWindow(sceneWindow.GetNameID(), dsIdUp);
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

        contentBrowserWindow.resetDockingLayout = true;
        logWindow.resetDockingLayout = true;
        for (auto& sceneWindow : sceneWindows) {
            sceneWindow.resetDockingLayout = true;
        }

        resetDockspaceLayout = false;

    }

    void App::SubscribeToResourceEvents() {

        ResourceManager<Scene::Scene>::Subscribe(ResourceTopic::ResourceCreate,
            [&](Ref<Resource<Scene::Scene>>& scene) {
            sceneWindows.emplace_back(ResourceHandle<Scene::Scene>(scene));
            Singletons::config->openedScenes.push_back(ResourceHandle<Scene::Scene>(scene));

            windowsToAddToNode.emplace_back(sceneWindows.back().GetNameID());
        });

        // Also kind of a resource event
        Events::EventManager::DropEventDelegate.Subscribe([](Events::DropEvent event) {
            if (!event.file.empty())
                FileImporter::ImportFile(event.file);
        });

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new Atlas::Editor::App();

}
