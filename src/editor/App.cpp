#include "App.h"
#include "ui/panels/PopupPanels.h"

#include <chrono>
#include <thread>

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
    .assetDirectory = "../../data",
    .shaderDirectory = "shader"
};

namespace Atlas::Editor {

    void App::LoadContent() {

        // Create a
        sceneWindows.emplace_back(ResourceHandle<Scene::Scene>());

        auto icon = Atlas::Texture::Texture2D("icon.png");
        window.SetIcon(&icon);

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        imguiWrapper.Load(&window);

        SubscribeToResourceEvents();

    }

    void App::UnloadContent() {

        imguiWrapper.Unload();

    }

    void App::Update(float deltaTime) {

        const ImGuiIO &io = ImGui::GetIO();

        imguiWrapper.Update(&window, deltaTime);

        if (io.WantCaptureMouse) {

        } else {

        }

    }

    void App::Render(float deltaTime) {

        auto windowFlags = window.GetFlags();
        if (windowFlags & AE_WINDOW_HIDDEN || windowFlags & AE_WINDOW_MINIMIZED || !(windowFlags & AE_WINDOW_SHOWN)) {
            return;
        }

        ImGui::NewFrame();

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
                ImGui::MenuItem("Open project", NULL, &openProject);
                ImGui::MenuItem("Save project", NULL, &saveProject);
                ImGui::Separator();
                ImGui::MenuItem("New scene", NULL, &newScene);
                ImGui::Separator();
                ImGui::MenuItem("Import files", NULL, &importFiles);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Reset layout", NULL, &resetDockspaceLayout);
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

        /*
        if (ImGui::Begin("Viewer", nullptr, 0)) {
            static float x = 0.0f;
            ImGui::SliderFloat("Testtest", &x, 0.0f, 1.0f);

            auto region = ImGui::GetContentRegionAvail();
            auto ratio = float(map->width) / float(map->height);
            region.x /= ratio;
            if (region.x > region.y) {
                region.x = region.y * ratio;
            } else {
                region.y = region.x;
                region.x = region.x * ratio;
            }

            ImGui::Image(texture, region);
        }

                 ImGui::End();
        */

        ImGui::End();

        ImGui::Render();
        imguiWrapper.Render(true);

    }

    void App::SetupMainDockspace(ImGuiID dsID) {

        ImGui::SetWindowSize(ImVec2(370.0f, 680.0f));
        ImGui::SetWindowPos(ImVec2(640.0f, 55.0f));

        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID);

        ImGuiID dsIdUp, dsIdDown;
        ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Up, 0.8f, &dsIdUp, &dsIdDown);

        ImGuiDockNode* upperNode = ImGui::DockBuilderGetNode(dsIdUp);

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

        resetDockspaceLayout = false;

    }

    void App::SubscribeToResourceEvents() {

        ResourceManager<Scene::Scene>::Subscribe([&](Ref<Resource<Scene::Scene>>& scene) {
            sceneWindows.emplace_back(ResourceHandle<Scene::Scene>(scene));

            windowsToAddToNode.emplace_back(sceneWindows.back().GetNameID());
        });

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new Atlas::Editor::App();

}
