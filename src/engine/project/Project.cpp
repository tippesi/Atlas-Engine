#include "Project.h"
#include "ProjectSerializer.h"

#include "../Log.h"
#include "../loader/AssetLoader.h"
#include "../scene/Scene.h"
#include "../streaming/StreamingManager.h"
#include "../jobsystem/JobSystem.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Atlas {

    Ref<Project> Project::active;

    Ref<Project> Project::Load(const std::string& path) {

        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            Log::Error("Couldn't open project file " + path);
            return nullptr;
        }

        auto j = json::parse(stream, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            Log::Error("Project file " + path + " isn't valid json");
            return nullptr;
        }

        auto project = CreateRef<Project>();
        from_json(j, *project);
        project->path = fs::absolute(path).lexically_normal().generic_string();

        return project;

    }

    Ref<Project> Project::Create(const std::string& directory, const std::string& name,
        const std::string& templateDirectory, const std::string& standardDataDirectory) {

        auto target = fs::absolute(fs::path(directory) / name);
        std::error_code error;
        if (fs::exists(target, error) && !fs::is_empty(target, error)) {
            Log::Error("Can't create the project " + name + ", " + target.generic_string() + " isn't empty");
            return nullptr;
        }

        Ref<Project> templateProject;
        for (const auto& entry : fs::directory_iterator(templateDirectory, error)) {
            if (entry.path().extension() == ".aeproject") {
                templateProject = Load(entry.path().string());
                break;
            }
        }
        if (!templateProject) {
            Log::Error("The template " + templateDirectory + " has no project file");
            return nullptr;
        }

        auto project = CreateRef<Project>(*templateProject);
        project->name = name;
        project->path = (target / (name + ".aeproject")).generic_string();
        auto assetDirectory = fs::path(project->GetAssetDirectory());

        auto copy = [&](const fs::path& from, const fs::path& to) {
            fs::create_directories(to.parent_path(), error);
            fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
            if (error)
                Log::Error("Couldn't copy " + from.generic_string() + " to " + to.generic_string() + ": " + error.message());
            return !error;
            };

        auto templateFile = fs::path(templateDirectory) / "template.json";
        if (fs::exists(templateFile, error)) {
            std::ifstream stream(templateFile, std::ios::binary);
            auto j = json::parse(stream, nullptr, false);
            if (j.is_discarded()) {
                Log::Error("Template file " + templateFile.generic_string() + " isn't valid json");
                return nullptr;
            }
            for (const auto& standardPath : j.value("standardData", std::vector<std::string>())) {
                if (!copy(fs::path(standardDataDirectory) / standardPath, assetDirectory / standardPath))
                    return nullptr;
            }
        }

        // Files of the user who saved the template and the project file, which is saved with the new name
        for (const auto& entry : fs::directory_iterator(templateDirectory, error)) {
            auto fileName = entry.path().filename().string();
            if (fileName == ".atlas" || fileName == ".cache" || fileName == "template.json" ||
                entry.path().extension() == ".aeproject")
                continue;
            if (!copy(entry.path(), target / fileName))
                return nullptr;
        }

        if (!project->Save())
            return nullptr;

        return project;

    }

    bool Project::Save() {

        json j = *this;
        if (!Loader::AssetLoader::WriteJson(path, j, Loader::JsonFormat::Indented)) {
            Log::Error("Couldn't write project file " + path);
            return false;
        }

        engineVersion = j["engineVersion"].get<std::string>();
        return true;

    }

    void Project::Activate(const Ref<Project>& project) {

        // Loads still running would read paths of the new directory, the resources of the old one are dropped
        JobSystem::WaitAll();
        active = project;
        if (project)
            Loader::AssetLoader::SetAssetDirectory(project->GetAssetDirectory());

    }

    void Project::ApplySettings() const {

        Streaming::StreamingManager::settings = settings.streaming;

    }

    void Project::ApplyTo(Scene::Scene& scene) const {

        scene.occlusionCulling = settings.occlusionCulling;
        scene.screenSpaceRefraction = settings.screenSpaceRefraction;
        scene.meshLodErrorScale = settings.meshLodErrorScale;
        scene.postProcessing.upscaler = settings.upscaler;
        scene.binaryColumns = settings.binarySceneColumns;

    }

    bool Project::AdoptSceneSettings(const Scene::Scene& scene) {

        if (hasSceneSettings)
            return false;

        settings.occlusionCulling = scene.occlusionCulling;
        settings.screenSpaceRefraction = scene.screenSpaceRefraction;
        settings.meshLodErrorScale = scene.meshLodErrorScale;
        settings.upscaler = scene.postProcessing.upscaler;
        settings.binarySceneColumns = scene.binaryColumns;
        hasSceneSettings = true;
        return true;

    }

    std::string Project::GetDirectory() const {

        return fs::path(path).parent_path().generic_string();

    }

    std::string Project::GetAssetDirectory() const {

        // "." would otherwise leave a trailing separator
        auto directory = (fs::path(GetDirectory()) / assetDirectory).lexically_normal();
        if (!directory.has_filename())
            directory = directory.parent_path();
        return directory.generic_string();

    }

}
