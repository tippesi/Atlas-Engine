#include "AssetRenamer.h"
#include "AssetReferenceIndex.h"
#include "../ContentDiscovery.h"

#include "Font.h"
#include "Material.h"
#include "animation/Animation.h"
#include "audio/AudioData.h"
#include "common/Path.h"
#include "materialgraph/MaterialGraph.h"
#include "mesh/Impostor.h"
#include "mesh/Mesh.h"
#include "resource/ResourceManager.h"
#include "scene/components/LuaScriptComponent.h"
#include "scripting/Script.h"
#include "terrain/Terrain.h"
#include "texture/Cubemap.h"
#include "texture/Texture2D.h"

namespace Atlas::Editor {

    std::unordered_map<std::string, std::string> AssetRenamer::renamedPaths;

    void AssetRenamer::RenameResources(const std::string& oldPath, const std::string& newPath) {

        AddRenamedPaths(oldPath, newPath);

        ResourceManager<Mesh::Mesh>::RenameResources(oldPath, newPath);
        ResourceManager<Mesh::Impostor>::RenameResources(oldPath, newPath);
        ResourceManager<Material>::RenameResources(oldPath, newPath);
        ResourceManager<MaterialGraph::Graph>::RenameResources(oldPath, newPath);
        ResourceManager<Texture::Texture2D>::RenameResources(oldPath, newPath);
        ResourceManager<Texture::Cubemap>::RenameResources(oldPath, newPath);
        ResourceManager<Audio::AudioData>::RenameResources(oldPath, newPath);
        ResourceManager<Animation::Animation>::RenameResources(oldPath, newPath);
        ResourceManager<Scripting::Script>::RenameResources(oldPath, newPath);
        ResourceManager<Terrain::Terrain>::RenameResources(oldPath, newPath);
        ResourceManager<Scene::Scene>::RenameResources(oldPath, newPath);
        ResourceManager<Font>::RenameResources(oldPath, newPath);

        // Assets which are only named by their path
        auto rename = [&](std::string& path) { path = Common::Path::Rename(path, oldPath, newPath); };
        for (auto& material : ResourceManager<Material>::GetResources()) {
            if (!material.IsLoaded())
                continue;
            rename(material->graphBake.meshPath);
            for (auto& [_, path] : material->graphBake.unbaked.maps)
                rename(path);
        }
        for (auto& terrain : ResourceManager<Terrain::Terrain>::GetResources()) {
            if (!terrain.IsLoaded())
                continue;
            rename(terrain->filename);
            rename(terrain->shoreLinePath);
        }

    }

    json AssetRenamer::UpdatePaths(json state) {

        if (renamedPaths.empty())
            return state;

        auto update = [](auto&& self, json& value) -> void {
            if (value.is_structured()) {
                for (auto& element : value)
                    self(self, element);
            }
            else if (value.is_string()) {
                auto renamed = renamedPaths.find(Common::Path::ToLower(value.get<std::string>()));
                if (renamed != renamedPaths.end())
                    value = renamed->second;
            }
            };
        update(update, state);
        return state;

    }

    void AssetRenamer::AddRenamedPaths(const std::string& oldPath, const std::string& newPath) {

        // The discovery still has the old paths
        for (const auto& content : ContentDiscovery::GetAllContent()) {
            auto path = Common::Path::Rename(content.assetPath, oldPath, newPath);
            if (path == content.assetPath)
                continue;

            for (auto& [_, renamedPath] : renamedPaths)
                if (renamedPath == content.assetPath)
                    renamedPath = path;
            renamedPaths[Common::Path::ToLower(content.assetPath)] = path;
            // It's the file's own path again, or another one
            renamedPaths.erase(Common::Path::ToLower(path));
        }

    }

    void AssetRenamer::RenameInScene(Scene::Scene& scene, const std::string& oldPath, const std::string& newPath) {

        auto scripts = scene.GetSubset<LuaScriptComponent>();
        for (auto entity : scripts) {
            for (auto& [name, property] : scripts.Get(entity).properties) {
                if (property.type != LuaScriptComponent::PropertyType::String || !AssetReferenceIndex::IsAsset(property.stringValue))
                    continue;
                auto path = Common::Path::Rename(property.stringValue, oldPath, newPath);
                if (path != property.stringValue)
                    property.SetValue(path);
            }
        }

    }

}
