#include "Serialization.h"
#include "Singletons.h"
#include "FileImporter.h"
#include "Notifications.h"

#include "common/SerializationHelper.h"
#include "tools/ToolSerializer.h"
#include "scene/SceneSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "Log.h"
#include "profiler/HitchRecorder.h"

#include <algorithm>
#include <cctype>
#include "loader/AssetLoader.h"

namespace Atlas::Editor {

    const std::string Serialization::configPath = ".config/";
    const std::string Serialization::configFilename = configPath + "config.json";
    const std::string Serialization::contentBrowserWindowFilename = configPath + "contentBrowserWindowConfig.json";

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

        json j = {
            { "darkMode", config->darkMode },
            { "pathTrace", config->pathTrace },
            { "vsync", config->vsync },
            { "uiScale", config->uiScale },
            { "scenes", scenePaths },
            { "recentScenes", config->recentScenes },
            { "statusBarDrawers", config->statusBarDrawers },
            { "hitches", hitches }
        };

        TryWriteToFile(configFilename, to_string(j));

    }

    void Serialization::DeserializeConfig() {

       auto serialized = TryReadFromFile(configFilename);

        if (serialized.empty())
            return;

        json j = json::parse(serialized);

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
            FileImporter::ImportFile(scenePath);

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

        json camera;
        std::set<ECS::Entity> insertedEntites;
        Scene::EntityToJson(camera, sceneWindow->cameraEntity, sceneWindow->scene.Get().get(), insertedEntites);

        json j = {
            { "resolutionScale", sceneWindow->resolutionScale },
            { "snappingEnabled", sceneWindow->guizmo.snappingEnabled },
            { "translationSnap", sceneWindow->guizmo.translationSnap },
            { "rotationSnap", sceneWindow->guizmo.rotationSnap },
            { "scaleSnap", sceneWindow->guizmo.scaleSnap },
            { "cameraMovementSpeed", sceneWindow->cameraMovementSpeed },
            { "showGrid", sceneWindow->showGrid },
            { "cameraRotationSpeed", sceneWindow->cameraRotationSpeed },
            { "depthTestBoundingVolumes", sceneWindow->depthTestBoundingVolumes },
            { "showPhysics", sceneWindow->showPhysics },
            { "depthTestPhysics", sceneWindow->depthTestPhysics },
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

        TryWriteToFile(GetSceneWindowConfigFilename(sceneWindow->scene), to_string(j));

    }

    Ref<UI::SceneWindow> Serialization::DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle) {

        if (!handle.IsLoaded())
            return nullptr;

        auto sceneWindow = CreateRef<UI::SceneWindow>(handle, true);

        // Configs written before they were named by the file are named by the scene, saving moves them over
        auto serialized = TryReadFromFile(GetSceneWindowConfigFilename(handle));
        if (serialized.empty())
            serialized = TryReadFromFile(configPath + handle->name + "WindowConfig.json");

        if (serialized.empty())
            return sceneWindow;

        json j = json::parse(serialized);

        json camera;

        try_get_json(j, "resolutionScale", sceneWindow->resolutionScale);
        try_get_json(j, "snappingEnabled", sceneWindow->guizmo.snappingEnabled);
        try_get_json(j, "translationSnap", sceneWindow->guizmo.translationSnap);
        try_get_json(j, "rotationSnap", sceneWindow->guizmo.rotationSnap);
        try_get_json(j, "scaleSnap", sceneWindow->guizmo.scaleSnap);
        try_get_json(j, "cameraMovementSpeed", sceneWindow->cameraMovementSpeed);
        try_get_json(j, "showGrid", sceneWindow->showGrid);
        try_get_json(j, "cameraRotationSpeed", sceneWindow->cameraRotationSpeed);
        try_get_json(j, "depthTestBoundingVolumes", sceneWindow->depthTestBoundingVolumes);
        try_get_json(j, "showPhysics", sceneWindow->showPhysics);
        try_get_json(j, "depthTestPhysics", sceneWindow->depthTestPhysics);
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

        TryWriteToFile(contentBrowserWindowFilename, to_string(j));

    }

    void Serialization::DeserializeContentBrowserWindow(UI::ContentBrowserWindow& contentBrowserWindow) {

        auto serialized = TryReadFromFile(contentBrowserWindowFilename);

        if (serialized.empty())
            return;

        json j = json::parse(serialized);

        if (j.contains("filterRecursively"))
            j.at("filterRecursively").get_to(contentBrowserWindow.filterRecursively);
        if (j.contains("searchRecursively"))
            j.at("searchRecursively").get_to(contentBrowserWindow.searchRecursively);
        if (j.contains("contentFilter"))
            j.at("contentFilter").get_to(contentBrowserWindow.contentFilter);
        // All types before animations and material graphs could be filtered
        if (contentBrowserWindow.contentFilter == (1 << 11) - 1 || contentBrowserWindow.contentFilter == (1 << 12) - 1)
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

        TryWriteToFile(GetDrawerWindowConfigFilename(drawerWindow), to_string(j));

    }

    void Serialization::DeserializeDrawerWindow(UI::DrawerWindow& drawerWindow) {

        auto serialized = TryReadFromFile(GetDrawerWindowConfigFilename(drawerWindow));

        if (serialized.empty())
            return;

        json j = json::parse(serialized);

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

    void Serialization::TryWriteToFile(const std::string& filename, const std::string& content) {

        auto fileStream = Loader::AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write config file");
            Notifications::Push({ .message = "Couldn't write the editor settings to " + filename, .type = NotificationType::Error });
            return;
        }

        fileStream << content;

        fileStream.close();

    }

    std::string Serialization::TryReadFromFile(const std::string& filename) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            return std::string();
        }

        std::string content((std::istreambuf_iterator<char>(fileStream)),
            std::istreambuf_iterator<char>());

        fileStream.close();

        return content;

    }

}