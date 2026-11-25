#include "Serialization.h"
#include "Singletons.h"
#include "FileImporter.h"
#include "Notifications.h"

#include "common/Path.h"
#include "common/SerializationHelper.h"
#include "tools/ToolSerializer.h"
#include "scene/SceneSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "Log.h"
#include "loader/AssetLoader.h"
#include "profiler/HitchRecorder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Atlas::Editor {

    std::string Serialization::configPath = ".config/";
    std::unordered_map<std::string, json> Serialization::writtenConfigs;
    std::unordered_set<std::string> Serialization::failedConfigs;

    void Serialization::SetConfigDirectory(const std::string& directory) {

        configPath = directory.ends_with('/') ? directory : directory + "/";

    }

    void Serialization::SerializeConfig() {

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;
        for (const auto& scene : config->openedScenes)
            scenePaths.push_back(scene.GetResource()->path);

        auto detectorToJson = [](const HitchRecorder::Detector& detector) {
            return json {
                { "enable", detector.enable },
                { "metric", static_cast<int32_t>(detector.metric) },
                { "trigger", static_cast<int32_t>(detector.trigger) },
                { "minimumTime", detector.minimumTime },
                { "relativeThreshold", detector.relativeThreshold }
            };
            };

        auto hitchSettings = HitchRecorder::GetSettings();
        json hitches = {
            { "enable", hitchSettings.enable },
            { "cpu", detectorToJson(hitchSettings.cpu) },
            { "gpu", detectorToJson(hitchSettings.gpu) },
            { "maxHitches", hitchSettings.maxHitches }
        };

        auto shortcuts = json::object();
        for (const auto& [command, shortcut] : config->shortcuts)
            shortcuts[command] = UI::CommandPalette::GetShortcutName(shortcut);

        json j = {
            { "darkMode", config->darkMode },
            { "pathTrace", config->pathTrace },
            { "vsync", config->vsync },
            { "uiScale", config->uiScale },
            { "scenes", scenePaths },
            { "recentScenes", config->recentScenes },
            { "shortcuts", shortcuts },
            { "statusBarDrawers", config->statusBarDrawers },
            { "hitches", hitches }
        };

        TryWriteToFile(configPath + "config.json", j);

    }

    void Serialization::DeserializeConfig() {

        auto j = TryReadJson(configPath + "config.json");

        if (j.empty())
            return;

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;

        if (j.contains("darkMode"))
            j.at("darkMode").get_to(config->darkMode);
        if (j.contains("pathTrace"))
            j.at("pathTrace").get_to(config->pathTrace);
        if (j.contains("vsync"))
            j.at("vsync").get_to(config->vsync);
        if (j.contains("uiScale"))
            j.at("uiScale").get_to(config->uiScale);
        if (j.contains("scenes"))
            j.at("scenes").get_to(scenePaths);
        if (j.contains("recentScenes"))
            j.at("recentScenes").get_to(config->recentScenes);
        if (j.contains("shortcuts")) {
            for (const auto& [command, name] : j.at("shortcuts").items())
                config->shortcuts[command] = UI::CommandPalette::ParseShortcut(name.get<std::string>());
        }
        if (j.contains("statusBarDrawers"))
            j.at("statusBarDrawers").get_to(config->statusBarDrawers);

        if (j.contains("hitches")) {
            auto detectorFromJson = [](const json& j, HitchRecorder::Detector& detector) {
                int32_t metric = static_cast<int32_t>(detector.metric);
                int32_t trigger = static_cast<int32_t>(detector.trigger);
                try_get_json(j, "enable", detector.enable);
                try_get_json(j, "metric", metric);
                try_get_json(j, "trigger", trigger);
                try_get_json(j, "minimumTime", detector.minimumTime);
                try_get_json(j, "relativeThreshold", detector.relativeThreshold);
                detector.metric = static_cast<HitchRecorder::Metric>(metric);
                detector.trigger = static_cast<HitchRecorder::Trigger>(trigger);
                };

            const auto& hitches = j["hitches"];
            auto hitchSettings = HitchRecorder::GetSettings();
            try_get_json(hitches, "enable", hitchSettings.enable);
            try_get_json(hitches, "maxHitches", hitchSettings.maxHitches);
            if (hitches.contains("cpu"))
                detectorFromJson(hitches["cpu"], hitchSettings.cpu);
            if (hitches.contains("gpu"))
                detectorFromJson(hitches["gpu"], hitchSettings.gpu);
            HitchRecorder::SetSettings(hitchSettings);
        }

        // No need to add it to the config, will be done through resource events
        for (const auto& scenePath : scenePaths)
            FileImporter::ImportFile<Scene::Scene>(scenePath);

    }

    std::string Serialization::GetSceneWindowConfigFilename(const ResourceHandle<Scene::Scene>& scene) {

        // The path without its extension, e.g. scenes_sponzaWindowConfig.json for scenes/sponza.aescene
        auto key = UI::SceneWindow::GetScenePath(scene);
        key = key.substr(0, key.rfind('.'));
        std::replace_if(key.begin(), key.end(), [](char c) { return c == '/' || c == '\\' || c == ':'; }, '_');
        return configPath + key + "WindowConfig.json";

    }

    void Serialization::SerializeSceneWindow(const Ref<UI::SceneWindow>& sceneWindow) {

        if (!sceneWindow->scene.IsLoaded())
            return;

        // The window opens with its own camera again
        sceneWindow->ReturnToEditorCamera();

        json camera;
        std::set<ECS::Entity> insertedEntites;
        Scene::EntityToJson(camera, sceneWindow->cameraEntity, sceneWindow->scene.Get().get(), insertedEntites);

        json bookmarks = json::array();
        for (const auto& [index, bookmark] : sceneWindow->bookmarks)
            bookmarks.push_back({ { "index", index }, { "location", bookmark.location }, { "rotation", bookmark.rotation } });

        const auto& orthographicView = sceneWindow->orthographicView;

        json j = {
            { "resolutionScale", sceneWindow->resolutionScale },
            { "snappingEnabled", sceneWindow->guizmo.snappingEnabled },
            { "translationSnap", sceneWindow->guizmo.translationSnap },
            { "rotationSnap", sceneWindow->guizmo.rotationSnap },
            { "scaleSnap", sceneWindow->guizmo.scaleSnap },
            { "guizmoMode", sceneWindow->guizmo.mode },
            { "objectSpace", sceneWindow->guizmo.objectSpace },
            { "alignToSurface", sceneWindow->guizmo.alignToSurface },
            { "bookmarks", bookmarks },
            { "orthographicView", {
                { "active", orthographicView.active },
                { "focusDistance", orthographicView.focusDistance },
                { "fieldOfView", orthographicView.fieldOfView },
                { "nearPlane", orthographicView.nearPlane },
                { "farPlane", orthographicView.farPlane }
            } },
            { "cameraMovementSpeed", sceneWindow->cameraMovementSpeed },
            { "showGrid", sceneWindow->showGrid },
            { "cameraRotationSpeed", sceneWindow->cameraRotationSpeed },
            { "depthTestBoundingVolumes", sceneWindow->overlays.depthTest },
            { "showPhysics", sceneWindow->showPhysics },
            { "depthTestPhysics", sceneWindow->depthTestPhysics },
            { "overlays", sceneWindow->overlays.shownKinds },
            { "overlayDistance", sceneWindow->overlays.distance },
            { "iconSize", sceneWindow->overlays.iconSize },
            { "occludedIconAlpha", sceneWindow->viewportPanel.occludedSpriteAlpha },
            { "physicsDebugDrawSettings", sceneWindow->physicsDebugDrawSettings },
            { "playMaximized", sceneWindow->playMaximized },
            { "perfOverlayMaximized", sceneWindow->perfOverlayMaximized },
            { "terrainGenerator", sceneWindow->scenePropertiesPanel.terrainPanel.terrainGenerator },
            { "vegetationGenerator", sceneWindow->scenePropertiesPanel.terrainPanel.vegetationGeneratorPanel.vegetationGenerator },
            { "camera", camera }
        };

        // Hidden components are saved by their key, new component types are shown
        json hiddenComponents = json::array();
        const auto& componentTypes = Scene::GetComponentTypes();
        for (size_t i = 0; i < componentTypes.size(); i++)
            if (!(sceneWindow->sceneHierarchyPanel.hierarchyFilter & (1u << uint32_t(i))))
                hiddenComponents.push_back(componentTypes[i].key);
        j["hierarchyHiddenComponents"] = hiddenComponents;

        TryWriteToFile(GetSceneWindowConfigFilename(sceneWindow->scene), j);

    }

    Ref<UI::SceneWindow> Serialization::DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle) {

        if (!handle.IsLoaded())
            return nullptr;

        auto sceneWindow = CreateRef<UI::SceneWindow>(handle, true);

        // Configs written before they were named by the file are named by the scene, saving moves them over
        auto j = TryReadJson(GetSceneWindowConfigFilename(handle));
        if (j.empty())
            j = TryReadJson(configPath + handle->name + "WindowConfig.json");

        if (j.empty())
            return sceneWindow;

        json camera;

        try_get_json(j, "resolutionScale", sceneWindow->resolutionScale);
        try_get_json(j, "snappingEnabled", sceneWindow->guizmo.snappingEnabled);
        try_get_json(j, "translationSnap", sceneWindow->guizmo.translationSnap);
        try_get_json(j, "rotationSnap", sceneWindow->guizmo.rotationSnap);
        try_get_json(j, "scaleSnap", sceneWindow->guizmo.scaleSnap);
        try_get_json(j, "guizmoMode", sceneWindow->guizmo.mode);
        try_get_json(j, "objectSpace", sceneWindow->guizmo.objectSpace);
        try_get_json(j, "alignToSurface", sceneWindow->guizmo.alignToSurface);
        if (j.contains("bookmarks")) {
            for (const auto& bookmark : j["bookmarks"])
                sceneWindow->bookmarks[bookmark.at("index")] = { .location = bookmark.at("location"), .rotation = bookmark.at("rotation") };
        }
        if (j.contains("orthographicView")) {
            auto& orthographicView = sceneWindow->orthographicView;
            try_get_json(j["orthographicView"], "active", orthographicView.active);
            try_get_json(j["orthographicView"], "focusDistance", orthographicView.focusDistance);
            try_get_json(j["orthographicView"], "fieldOfView", orthographicView.fieldOfView);
            try_get_json(j["orthographicView"], "nearPlane", orthographicView.nearPlane);
            try_get_json(j["orthographicView"], "farPlane", orthographicView.farPlane);
        }
        try_get_json(j, "cameraMovementSpeed", sceneWindow->cameraMovementSpeed);
        try_get_json(j, "showGrid", sceneWindow->showGrid);
        try_get_json(j, "cameraRotationSpeed", sceneWindow->cameraRotationSpeed);
        try_get_json(j, "depthTestBoundingVolumes", sceneWindow->overlays.depthTest);
        try_get_json(j, "showPhysics", sceneWindow->showPhysics);
        try_get_json(j, "depthTestPhysics", sceneWindow->depthTestPhysics);
        try_get_json(j, "overlays", sceneWindow->overlays.shownKinds);
        try_get_json(j, "overlayDistance", sceneWindow->overlays.distance);
        try_get_json(j, "iconSize", sceneWindow->overlays.iconSize);
        try_get_json(j, "occludedIconAlpha", sceneWindow->viewportPanel.occludedSpriteAlpha);
        try_get_json(j, "physicsDebugDrawSettings", sceneWindow->physicsDebugDrawSettings);
        try_get_json(j, "playMaximized", sceneWindow->playMaximized);
        try_get_json(j, "perfOverlayMaximized", sceneWindow->perfOverlayMaximized);
        try_get_json(j, "camera", camera);
        try_get_json(j, "terrainGenerator", sceneWindow->scenePropertiesPanel.terrainPanel.terrainGenerator);
        try_get_json(j, "vegetationGenerator", sceneWindow->scenePropertiesPanel.terrainPanel.vegetationGeneratorPanel.vegetationGenerator);
        std::vector<std::string> hiddenComponents;
        try_get_json(j, "hierarchyHiddenComponents", hiddenComponents);
        const auto& componentTypes = Scene::GetComponentTypes();
        for (size_t i = 0; i < componentTypes.size(); i++)
            if (std::find(hiddenComponents.begin(), hiddenComponents.end(), componentTypes[i].key) != hiddenComponents.end())
                sceneWindow->sceneHierarchyPanel.hierarchyFilter &= ~(1u << uint32_t(i));

        sceneWindow->cameraEntity = sceneWindow->scene->CreateEntity();
        Scene::EntityFromJson(camera, sceneWindow->cameraEntity, sceneWindow->scene.Get().get(), false);

        // When closing the application while playing the entity is saved in the wrong state
        sceneWindow->cameraEntity.GetComponent<CameraComponent>().isMain = true;

        return sceneWindow;

    }

    void Serialization::SerializeContentBrowserWindow(const UI::ContentBrowserWindow& contentBrowserWindow) {

        json j = {
            { "filterRecursively", contentBrowserWindow.filterRecursively },
            { "searchRecursively", contentBrowserWindow.searchRecursively },
            { "contentFilter", contentBrowserWindow.contentFilter },
            { "iconSize", contentBrowserWindow.iconSize },
            { "folderPanelWidth", contentBrowserWindow.folderPanelWidth },
            { "listView", contentBrowserWindow.listView },
            { "showPreviews", contentBrowserWindow.showPreviews },
            { "pinned", contentBrowserWindow.pinned },
            { "drawerHeight", contentBrowserWindow.height },
        };

        TryWriteToFile(configPath + "contentBrowserWindowConfig.json", j);

    }

    void Serialization::DeserializeContentBrowserWindow(UI::ContentBrowserWindow& contentBrowserWindow) {

        auto j = TryReadJson(configPath + "contentBrowserWindowConfig.json");

        if (j.empty())
            return;

        if (j.contains("filterRecursively"))
            j.at("filterRecursively").get_to(contentBrowserWindow.filterRecursively);
        if (j.contains("searchRecursively"))
            j.at("searchRecursively").get_to(contentBrowserWindow.searchRecursively);
        if (j.contains("contentFilter"))
            j.at("contentFilter").get_to(contentBrowserWindow.contentFilter);
        // A filter showing all types of an older version shows the types added since then too
        auto filter = contentBrowserWindow.contentFilter;
        if (filter == (1u << 11) - 1 || filter == (1u << 12) - 1 || filter == (1u << 13) - 1 ||
            filter == (1u << 15) - 1)
            contentBrowserWindow.contentFilter = UI::ContentBrowserWindow::AllBit;
        if (j.contains("iconSize"))
            j.at("iconSize").get_to(contentBrowserWindow.iconSize);
        if (j.contains("folderPanelWidth"))
            j.at("folderPanelWidth").get_to(contentBrowserWindow.folderPanelWidth);
        if (j.contains("listView"))
            j.at("listView").get_to(contentBrowserWindow.listView);
        if (j.contains("showPreviews"))
            j.at("showPreviews").get_to(contentBrowserWindow.showPreviews);
        if (j.contains("pinned"))
            j.at("pinned").get_to(contentBrowserWindow.pinned);
        if (j.contains("drawerHeight"))
            j.at("drawerHeight").get_to(contentBrowserWindow.height);
        contentBrowserWindow.show = contentBrowserWindow.pinned;
    }

    void Serialization::SerializeDrawerWindow(const UI::DrawerWindow& drawerWindow) {

        json j = {
            { "pinned", drawerWindow.pinned },
            { "drawerHeight", drawerWindow.height },
        };

        TryWriteToFile(GetDrawerWindowConfigFilename(drawerWindow), j);

    }

    void Serialization::DeserializeDrawerWindow(UI::DrawerWindow& drawerWindow) {

        auto j = TryReadJson(GetDrawerWindowConfigFilename(drawerWindow));

        if (j.empty())
            return;

        if (j.contains("pinned"))
            j.at("pinned").get_to(drawerWindow.pinned);
        if (j.contains("drawerHeight"))
            j.at("drawerHeight").get_to(drawerWindow.height);
        drawerWindow.show = drawerWindow.pinned;

    }

    std::string Serialization::GetDrawerWindowConfigFilename(const UI::DrawerWindow& drawerWindow) {

        // E.g. logWindowConfig.json for the log
        auto name = drawerWindow.name;
        name[0] = char(std::tolower(name[0]));
        return configPath + name + "WindowConfig.json";

    }

    void Serialization::TryWriteToFile(const std::string& filename, json& j) {

        auto written = writtenConfigs.find(filename);
        if (written != writtenConfigs.end() && written->second == j)
            return;

        writtenConfigs[filename] = j;

        if (Loader::AssetLoader::WriteJson(filename, j, Loader::JsonFormat::Compact)) {
            failedConfigs.erase(filename);
            return;
        }

        // It's tried again with the next save, the user knows already
        writtenConfigs.erase(filename);
        if (failedConfigs.insert(filename).second) {
            Log::Error("Couldn't write config file " + filename);
            Notifications::Push({ .message = "Couldn't write the editor settings to " + filename, .type = NotificationType::Error });
        }

    }

    json Serialization::TryReadJson(const std::string& filename) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open())
            return json();

        auto j = json::parse(fileStream, nullptr, false);
        if (!j.is_discarded())
            return j;

        // The next save doesn't overwrite what can still be recovered
        fileStream.close();
        std::error_code error;
        std::filesystem::rename(path, path + ".damaged", error);

        auto message = "The editor settings in " + filename + " are damaged and kept as " +
            Common::Path::GetFileName(filename) + ".damaged, the defaults are used";
        Log::Warning(message);
        Notifications::Push({ .message = message, .type = NotificationType::Warning });

        return json();

    }

}