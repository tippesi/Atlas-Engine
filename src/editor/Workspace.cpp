#include "Workspace.h"
#include "DataCreator.h"
#include "FileImporter.h"
#include "Notifications.h"
#include "Singletons.h"
#include "tools/AssetReferenceIndex.h"
#include "tools/AssetRenamer.h"

#include "common/Path.h"

#include <imgui_internal.h>

#include <algorithm>

namespace Atlas::Editor {

    void Workspace::Close() {

        sceneWindows.clear();
        assetEditorWindows.clear();
        codeEditorWindows.clear();
        activeAssetEditorWindow.reset();
        activeCodeEditorWindow.reset();
        activeSceneIdx = 0;
        drawerDockNodeID = 0;
        projectSettingsWindow.history.Clear();
        projectSettingsWindow.hasUnsavedChanges = false;
        projectSettingsWindow.show = false;

    }

    bool Workspace::IsPlayingMaximized() const {

        auto sceneWindow = GetActiveSceneWindow();
        return sceneWindow && sceneWindow->isPlaying && sceneWindow->playMaximized;

    }

    Ref<UI::SceneWindow> Workspace::GetActiveSceneWindow() const {

        if (sceneWindows.empty())
            return nullptr;

        return sceneWindows[std::min(activeSceneIdx, sceneWindows.size() - 1)];

    }

    const UndoHistory* Workspace::GetActiveHistory() const {

        if (projectSettingsWindow.inFocus)
            return &projectSettingsWindow.history;

        if (activeAssetEditorWindow)
            return &activeAssetEditorWindow->history;

        auto sceneWindow = GetActiveSceneWindow();
        if (!sceneWindow || sceneWindow->isPlaying)
            return nullptr;
        return &sceneWindow->history.history;

    }

    void Workspace::Undo() {

        if (projectSettingsWindow.inFocus) {
            projectSettingsWindow.history.Undo();
            return;
        }

        if (activeCodeEditorWindow) {
            activeCodeEditorWindow->codeEditor.Undo();
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

    void Workspace::Redo() {

        if (projectSettingsWindow.inFocus) {
            projectSettingsWindow.history.Redo();
            return;
        }

        if (activeCodeEditorWindow) {
            activeCodeEditorWindow->codeEditor.Redo();
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

    bool Workspace::CanUndo() const {

        if (activeCodeEditorWindow && !projectSettingsWindow.inFocus)
            return activeCodeEditorWindow->codeEditor.CanUndo();
        auto history = GetActiveHistory();
        return history && history->CanUndo();

    }

    bool Workspace::CanRedo() const {

        if (activeCodeEditorWindow && !projectSettingsWindow.inFocus)
            return activeCodeEditorWindow->codeEditor.CanRedo();
        auto history = GetActiveHistory();
        return history && history->CanRedo();

    }

    std::string Workspace::Open(const std::string& path) {

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

        auto isGraph = type == ContentType::MaterialGraph || type == ContentType::MaterialSubgraph;
        if (type != ContentType::Material && !isGraph && type != ContentType::Mesh) {
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
        else if (isGraph)
            assetEditorWindows.push_back(CreateRef<UI::MaterialEditorWindow>(FileImporter::ImportFile<MaterialGraph::Graph>(path), true));
        else
            assetEditorWindows.push_back(CreateRef<UI::MeshEditorWindow>(FileImporter::ImportFile<Mesh::Mesh>(path), true));
        // Docks next to the window which is still the active one
        OpenAsTab(*assetEditorWindows.back());
        activeAssetEditorWindow = assetEditorWindows.back();
        return {};

    }

    bool Workspace::IsWindowOpen(const Ref<UI::Window>& window) const {

        auto contains = [&](const auto& windows) {
            return std::any_of(windows.begin(), windows.end(), [&](const auto& open) { return open == window; });
            };
        return contains(sceneWindows) || contains(assetEditorWindows) || contains(codeEditorWindows);

    }

    void Workspace::SaveWindow(const Ref<UI::Window>& window) {

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

    std::pair<std::string, std::string> Workspace::GetWindowFile(const Ref<UI::Window>& window) const {

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

    void Workspace::RenamePaths(const std::string& oldPath, const std::string& newPath, bool updateReferences) {

        auto rename = [&](const std::string& path) { return Common::Path::Rename(path, oldPath, newPath); };
        for (const auto& window : codeEditorWindows) {
            // The file is changed as well, other changes of the window are still saved by the user
            auto text = updateReferences ? AssetReferenceIndex::RenameInContent(window->GetPath(), window->GetText(),
                oldPath, newPath) : window->GetText();
            window->SetPath(rename(window->GetPath()));
            if (text == window->GetText())
                continue;
            auto save = !window->hasUnsavedChanges;
            window->SetText(text);
            if (save)
                window->Save();
        }
        for (const auto& window : sceneWindows) {
            window->prefabPath = rename(window->prefabPath);
            if (updateReferences && window->scene.IsLoaded())
                AssetRenamer::RenameInScene(*window->scene.Get(), oldPath, newPath);
            window->history.ResetStates();
        }
        for (const auto& window : assetEditorWindows)
            window->AcceptState();

        auto& project = Singletons::project;
        if (!project)
            return;
        auto before = *project;
        project->startScene = rename(project->startScene);
        project->mainScript = rename(project->mainScript);
        if (project->startScene == before.startScene && project->mainScript == before.mainScript)
            return;

        // The file has its new name already, other changes of the project are still saved by the user
        auto save = !projectSettingsWindow.hasUnsavedChanges;
        projectSettingsWindow.RecordChange(project, before, "renamed paths");
        if (save)
            projectSettingsWindow.Save(project);

    }

    void Workspace::OpenAsTab(UI::Window& window) const {

        window.requestDocking = true;
        window.dockNodeID = GetActiveDockNodeID();
        window.requestFocus = true;

    }

    void Workspace::DockAsTab(UI::Window& window) {

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

    void Workspace::DockNextToScenes(UI::Window& window) {

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

    ImGuiID Workspace::GetActiveDockNodeID() const {

        const UI::Window* window = activeAssetEditorWindow.get();
        if (!window && !sceneWindows.empty())
            window = sceneWindows[std::min(activeSceneIdx, sceneWindows.size() - 1)].get();

        auto imguiWindow = window ? ImGui::FindWindowByName(window->GetNameID()) : nullptr;
        return imguiWindow ? imguiWindow->DockId : 0;

    }

    Workspace::Drawer* Workspace::FindDrawer(const std::string& name) {

        auto iter = std::find_if(drawers.begin(), drawers.end(), [&](const Drawer& drawer) { return name == drawer.name; });
        return iter != drawers.end() ? &*iter : nullptr;

    }

    void Workspace::UpdateDrawers() {

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

    void Workspace::PrepareDrawerDocking(ImGuiID dsID) {

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

    void Workspace::DockPinnedDrawer(UI::DrawerWindow& drawer) {

        // A new layout has its central node only after the first update, the request waits until then
        if (!drawer.requestDocking || !drawerDockNodeID)
            return;
        drawer.requestDocking = false;

        ImGui::SetNextWindowDockID(drawerDockNodeID, ImGuiCond_Always);

    }

}
