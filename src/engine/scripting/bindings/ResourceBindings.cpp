#include "ResourceBindings.h"

#include "audio/AudioData.h"
#include "Font.h"
#include "Material.h"
#include "Serializer.h"
#include "mesh/Mesh.h"
#include "animation/Animation.h"
#include "scene/Scene.h"
#include "scripting/Script.h"
#include "terrain/Terrain.h"
#include "texture/Texture2D.h"
#include "texture/Cubemap.h"

#include "loader/AssetLoader.h"
#include "loader/MaterialLoader.h"
#include "loader/MeshLoader.h"
#include "loader/AnimationLoader.h"
#include "loader/ModelImporter.h"
#include "loader/TerrainLoader.h"
#include "common/Path.h"

#include <filesystem>

namespace Atlas::Scripting::Bindings {

    // Scripts can only access files in the asset/data directory: no absolute paths, no ".."
    static bool IsSafePath(const std::string& path) {

        if (path.empty() || Common::Path::IsAbsolute(path))
            return false;

        for (const auto& part : std::filesystem::path(path))
            if (part == "..")
                return false;

        return true;

    }

    void GenerateAssetLoaderBindings(sol::table* ns) {

        auto type = ns->create_named("AssetLoader");

        type.set_function("GetAssetDirectory", &Loader::AssetLoader::GetAssetDirectory);
        type.set_function("GetDataDirectory", &Loader::AssetLoader::GetDataDirectory);
        type.set_function("GetFullPath", &Loader::AssetLoader::GetFullPath);
        type.set_function("GetRelativePath", &Loader::AssetLoader::GetRelativePath);
        type.set_function("IsFileInAssetDirectory", &Loader::AssetLoader::IsFileInAssetDirectory);

        type.set_function("FileExists", [](const std::string& filename) {
                return IsSafePath(filename) && Loader::AssetLoader::FileExists(filename);
            });

        // There are no streams in Lua, the whole content is read/written as a string
        type.set_function("ReadFile", [](const std::string& filename, sol::this_state state) -> sol::object {
                if (!IsSafePath(filename) || !Loader::AssetLoader::FileExists(filename))
                    return sol::lua_nil;
                auto stream = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);
                if (!stream.is_open())
                    return sol::lua_nil;
                auto content = Loader::AssetLoader::GetFileContent(stream);
                return sol::make_object(state, std::string(content.begin(), content.end()));
            });

        type.set_function("WriteFile", [](const std::string& filename, const std::string& content) {
                if (!IsSafePath(filename))
                    return false;
                auto stream = Loader::AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);
                if (!stream.is_open())
                    return false;
                stream.write(content.data(), std::streamsize(content.size()));
                return stream.good();
            });

        type.set_function("RemoveFile", [](const std::string& filename) {
                return IsSafePath(filename) && Loader::AssetLoader::RemoveFile(filename);
            });

        type.set_function("MakeDirectory", [](const std::string& directory) {
                if (IsSafePath(directory))
                    Loader::AssetLoader::MakeDirectory(directory);
            });

    }

    void GenerateModelImporterBindings(sol::table* ns) {

        auto type = ns->create_named("ModelImporter");

        auto importMeshOverload = sol::overload(
            [](const std::string& filename) {
                return Loader::ModelImporter::ImportMesh(filename);
            },
            [](const std::string& filename, bool saveToDisk) -> Ref<Mesh::Mesh> {
                if (saveToDisk && !IsSafePath(filename))
                    return nullptr;
                return Loader::ModelImporter::ImportMesh(filename, saveToDisk);
            },
            [](const std::string& filename, bool saveToDisk, int32_t maxTextureResolution) -> Ref<Mesh::Mesh> {
                if (saveToDisk && !IsSafePath(filename))
                    return nullptr;
                return Loader::ModelImporter::ImportMesh(filename, saveToDisk, maxTextureResolution);
            },
            [](const std::string& filename, Mesh::MeshMobility mobility) {
                return Loader::ModelImporter::ImportMesh(filename, mobility);
            },
            [](const std::string& filename, Mesh::MeshMobility mobility, bool saveToDisk) -> Ref<Mesh::Mesh> {
                if (saveToDisk && !IsSafePath(filename))
                    return nullptr;
                return Loader::ModelImporter::ImportMesh(filename, mobility, saveToDisk);
            },
            [](const std::string& filename, Mesh::MeshMobility mobility, bool saveToDisk, int32_t maxTextureResolution) -> Ref<Mesh::Mesh> {
                if (saveToDisk && !IsSafePath(filename))
                    return nullptr;
                return Loader::ModelImporter::ImportMesh(filename, mobility, saveToDisk, maxTextureResolution);
            }
        );

        type.set_function("ImportMesh", importMeshOverload);

        auto importAnimationsOverload = sol::overload(
            [](const std::string& filename) {
                return Loader::ModelImporter::ImportAnimations(filename);
            },
            [](const std::string& filename, bool saveToDisk) -> std::vector<Ref<Animation::Animation>> {
                if (saveToDisk && !IsSafePath(filename))
                    return {};
                return Loader::ModelImporter::ImportAnimations(filename, saveToDisk);
            }
        );

        type.set_function("ImportAnimations", importAnimationsOverload);

        type.set_function("ImportScene", [](const std::string& filename, vec3 min, vec3 max, int32_t depth,
            sol::optional<bool> saveToDisk, sol::optional<bool> makeMeshesStatic, sol::optional<bool> invertUVs,
            sol::optional<int32_t> maxTextureResolution) -> Ref<Scene::Scene> {
                if (saveToDisk.value_or(true) && !IsSafePath(filename))
                    return nullptr;
                return Loader::ModelImporter::ImportScene(filename, min, max, depth, saveToDisk.value_or(true),
                    makeMeshesStatic.value_or(false), invertUVs.value_or(false), maxTextureResolution.value_or(4096));
            });

    }

    void GenerateResourceManagerBindings(sol::table* ns) {

        GenerateResourceBinding<Audio::AudioData>(ns, "AudioResourceHandle");
        GenerateResourceBinding<Material>(ns, "MaterialResourceHandle");
        GenerateResourceBinding<Texture::Texture2D>(ns, "Texture2DResourceHandle");
        GenerateResourceBinding<Texture::Cubemap>(ns, "CubemapResourceHandle");
        GenerateResourceBinding<Mesh::Mesh>(ns, "MeshResourceHandle");
        GenerateResourceBinding<Animation::Animation>(ns, "AnimationResourceHandle");
        GenerateResourceBinding<Scripting::Script>(ns, "ScriptResourceHandle");
        GenerateResourceBinding<Font>(ns, "FontResourceHandle");
        GenerateResourceBinding<Scene::Scene>(ns, "SceneResourceHandle");
        GenerateResourceBinding<Terrain::Terrain>(ns, "TerrainResourceHandle");

        ns->new_enum<Texture::Wrapping>("Wrapping", {
            { "Repeat", Texture::Wrapping::Repeat },
            { "MirroredRepeat", Texture::Wrapping::MirroredRepeat },
            { "ClampToEdge", Texture::Wrapping::ClampToEdge },
            { "ClampToWhite", Texture::Wrapping::ClampToWhite },
            { "ClampToBlack", Texture::Wrapping::ClampToBlack }
            });

        ns->new_enum<Texture::Filtering>("Filtering", {
            { "Nearest", Texture::Filtering::Nearest },
            { "Linear", Texture::Filtering::Linear },
            { "MipMapNearest", Texture::Filtering::MipMapNearest },
            { "MipMapLinear", Texture::Filtering::MipMapLinear },
            { "Anisotropic", Texture::Filtering::Anisotropic }
            });

        // Text components scale the font by textScale / 16, e.g. ComputeDimensions(text, textScale / 16)
        ns->new_usertype<Font>("Font",
            "ComputeDimensions", [](Font& font, const std::string& text, float scale) {
                vec2 size;
                font.ComputeDimensions(text, scale, &size.x, &size.y);
                return size;
            },
            "GetKerning", &Font::GetKerning,
            "lineHeight", &Font::lineHeight
            );

        auto type = ns->create_named("ResourceManager");

        // Meshes, materials, scenes and terrains are loaded with their loader, all other resources with their constructor
        auto getOrLoadMeshOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Mesh::Mesh> {
                return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoader(filename, Loader::MeshLoader::LoadMesh, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Mesh::Mesh> {
                return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoader(filename, Loader::MeshLoader::LoadMesh, binaryJson);
            }
        );

        auto getOrLoadMeshAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Mesh::Mesh> {
                return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(filename, Loader::MeshLoader::LoadMesh, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Mesh::Mesh> {
                return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(filename, Loader::MeshLoader::LoadMesh, binaryJson);
            }
        );

        type.set_function("GetOrLoadMesh", getOrLoadMeshOverload);
        type.set_function("GetOrLoadMeshAsync", getOrLoadMeshAsyncOverload);
        type.set_function("GetMesh", [](const std::string& filename) {
                return ResourceManager<Mesh::Mesh>::GetResource(filename);
            });
        type.set_function("GetMeshes", []() {
                return ResourceManager<Mesh::Mesh>::GetResources();
            });
        type.set_function("AddMesh", [](const std::string& filename, Ref<Mesh::Mesh> mesh) {
                return ResourceManager<Mesh::Mesh>::AddResource(filename, mesh);
            });

        auto getOrLoadAnimationOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Animation::Animation> {
                return ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoader(filename, Loader::AnimationLoader::LoadAnimation, true);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Animation::Animation> {
                return ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoader(filename, Loader::AnimationLoader::LoadAnimation, binaryJson);
            }
        );

        auto getOrLoadAnimationAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Animation::Animation> {
                return ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoaderAsync(filename, Loader::AnimationLoader::LoadAnimation, true);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Animation::Animation> {
                return ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoaderAsync(filename, Loader::AnimationLoader::LoadAnimation, binaryJson);
            }
        );

        type.set_function("GetOrLoadAnimation", getOrLoadAnimationOverload);
        type.set_function("GetOrLoadAnimationAsync", getOrLoadAnimationAsyncOverload);
        type.set_function("GetAnimation", [](const std::string& filename) {
                return ResourceManager<Animation::Animation>::GetResource(filename);
            });
        type.set_function("GetAnimations", []() {
                return ResourceManager<Animation::Animation>::GetResources();
            });
        type.set_function("AddAnimation", [](const std::string& filename, Ref<Animation::Animation> animation) {
                return ResourceManager<Animation::Animation>::AddResource(filename, animation);
            });

        auto getOrLoadMaterialOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Material> {
                return ResourceManager<Material>::GetOrLoadResourceWithLoader(filename, Loader::MaterialLoader::LoadMaterial, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Material> {
                return ResourceManager<Material>::GetOrLoadResourceWithLoader(filename, Loader::MaterialLoader::LoadMaterial, binaryJson);
            }
        );

        auto getOrLoadMaterialAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Material> {
                return ResourceManager<Material>::GetOrLoadResourceWithLoaderAsync(filename, Loader::MaterialLoader::LoadMaterial, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Material> {
                return ResourceManager<Material>::GetOrLoadResourceWithLoaderAsync(filename, Loader::MaterialLoader::LoadMaterial, binaryJson);
            }
        );

        type.set_function("GetOrLoadMaterial", getOrLoadMaterialOverload);
        type.set_function("GetOrLoadMaterialAsync", getOrLoadMaterialAsyncOverload);
        type.set_function("GetMaterial", [](const std::string& filename) {
                return ResourceManager<Material>::GetResource(filename);
            });
        type.set_function("GetMaterials", []() {
                return ResourceManager<Material>::GetResources();
            });
        type.set_function("AddMaterial", [](const std::string& filename, Ref<Material> material) {
                return ResourceManager<Material>::AddResource(filename, material);
            });

        auto getOrLoadTexture2DOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResource(filename);
            },
            [](const std::string& filename, bool colorSpaceConversion) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResource(filename, colorSpaceConversion);
            },
            [](const std::string& filename, bool colorSpaceConversion, Texture::Wrapping wrapping,
                Texture::Filtering filtering) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResource(filename, colorSpaceConversion, wrapping, filtering);
            }
        );

        auto getOrLoadTexture2DAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResourceAsync(filename);
            },
            [](const std::string& filename, bool colorSpaceConversion) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResourceAsync(filename, colorSpaceConversion);
            },
            [](const std::string& filename, bool colorSpaceConversion, Texture::Wrapping wrapping,
                Texture::Filtering filtering) -> ResourceHandle<Texture::Texture2D> {
                return ResourceManager<Texture::Texture2D>::GetOrLoadResourceAsync(filename, colorSpaceConversion, wrapping, filtering);
            }
        );

        type.set_function("GetOrLoadTexture2D", getOrLoadTexture2DOverload);
        type.set_function("GetOrLoadTexture2DAsync", getOrLoadTexture2DAsyncOverload);
        type.set_function("GetTexture2D", [](const std::string& filename) {
                return ResourceManager<Texture::Texture2D>::GetResource(filename);
            });
        type.set_function("GetTexture2Ds", []() {
                return ResourceManager<Texture::Texture2D>::GetResources();
            });
        type.set_function("AddTexture2D", [](const std::string& filename, Ref<Texture::Texture2D> texture) {
                return ResourceManager<Texture::Texture2D>::AddResource(filename, texture);
            });

        type.set_function("GetOrLoadCubemap", [](const std::string& filename) -> ResourceHandle<Texture::Cubemap> {
                return ResourceManager<Texture::Cubemap>::GetOrLoadResource(filename);
            });
        type.set_function("GetOrLoadCubemapAsync", [](const std::string& filename) -> ResourceHandle<Texture::Cubemap> {
                return ResourceManager<Texture::Cubemap>::GetOrLoadResourceAsync(filename);
            });
        type.set_function("GetCubemap", [](const std::string& filename) {
                return ResourceManager<Texture::Cubemap>::GetResource(filename);
            });
        type.set_function("GetCubemaps", []() {
                return ResourceManager<Texture::Cubemap>::GetResources();
            });
        type.set_function("AddCubemap", [](const std::string& filename, Ref<Texture::Cubemap> cubemap) {
                return ResourceManager<Texture::Cubemap>::AddResource(filename, cubemap);
            });

        type.set_function("GetOrLoadScript", [](const std::string& filename) -> ResourceHandle<Scripting::Script> {
                return ResourceManager<Scripting::Script>::GetOrLoadResource(filename);
            });
        type.set_function("GetOrLoadScriptAsync", [](const std::string& filename) -> ResourceHandle<Scripting::Script> {
                return ResourceManager<Scripting::Script>::GetOrLoadResourceAsync(filename);
            });
        type.set_function("GetScript", [](const std::string& filename) {
                return ResourceManager<Scripting::Script>::GetResource(filename);
            });
        type.set_function("GetScripts", []() {
                return ResourceManager<Scripting::Script>::GetResources();
            });
        type.set_function("AddScript", [](const std::string& filename, Ref<Scripting::Script> script) {
                return ResourceManager<Scripting::Script>::AddResource(filename, script);
            });

        type.set_function("GetOrLoadAudio", [](const std::string& filename) -> ResourceHandle<Audio::AudioData> {
                return ResourceManager<Audio::AudioData>::GetOrLoadResource(filename);
            });
        type.set_function("GetOrLoadAudioAsync", [](const std::string& filename) -> ResourceHandle<Audio::AudioData> {
                return ResourceManager<Audio::AudioData>::GetOrLoadResourceAsync(filename);
            });
        type.set_function("GetAudio", [](const std::string& filename) {
                return ResourceManager<Audio::AudioData>::GetResource(filename);
            });
        type.set_function("GetAudios", []() {
                return ResourceManager<Audio::AudioData>::GetResources();
            });
        type.set_function("AddAudio", [](const std::string& filename, Ref<Audio::AudioData> audio) {
                return ResourceManager<Audio::AudioData>::AddResource(filename, audio);
            });

        auto getOrLoadFontOverload = sol::overload(
            [](const std::string& filename, float pixelSize, int32_t padding) -> ResourceHandle<Font> {
                return ResourceManager<Font>::GetOrLoadResource(filename, pixelSize, padding);
            },
            [](const std::string& filename, float pixelSize, int32_t padding, uint8_t edgeValue) -> ResourceHandle<Font> {
                return ResourceManager<Font>::GetOrLoadResource(filename, pixelSize, padding, edgeValue);
            }
        );

        auto getOrLoadFontAsyncOverload = sol::overload(
            [](const std::string& filename, float pixelSize, int32_t padding) -> ResourceHandle<Font> {
                return ResourceManager<Font>::GetOrLoadResourceAsync(filename, pixelSize, padding);
            },
            [](const std::string& filename, float pixelSize, int32_t padding, uint8_t edgeValue) -> ResourceHandle<Font> {
                return ResourceManager<Font>::GetOrLoadResourceAsync(filename, pixelSize, padding, edgeValue);
            }
        );

        type.set_function("GetOrLoadFont", getOrLoadFontOverload);
        type.set_function("GetOrLoadFontAsync", getOrLoadFontAsyncOverload);
        type.set_function("GetFont", [](const std::string& filename) {
                return ResourceManager<Font>::GetResource(filename);
            });
        type.set_function("GetFonts", []() {
                return ResourceManager<Font>::GetResources();
            });
        type.set_function("AddFont", [](const std::string& filename, Ref<Font> font) {
                return ResourceManager<Font>::AddResource(filename, font);
            });

        auto getOrLoadSceneOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Scene::Scene> {
                return ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoader(filename, Serializer::DeserializeScene, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Scene::Scene> {
                return ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoader(filename, Serializer::DeserializeScene, binaryJson);
            }
        );

        auto getOrLoadSceneAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Scene::Scene> {
                return ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoaderAsync(filename, Serializer::DeserializeScene, false);
            },
            [](const std::string& filename, bool binaryJson) -> ResourceHandle<Scene::Scene> {
                return ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoaderAsync(filename, Serializer::DeserializeScene, binaryJson);
            }
        );

        type.set_function("GetOrLoadScene", getOrLoadSceneOverload);
        type.set_function("GetOrLoadSceneAsync", getOrLoadSceneAsyncOverload);
        type.set_function("GetScene", [](const std::string& filename) {
                return ResourceManager<Scene::Scene>::GetResource(filename);
            });
        type.set_function("GetScenes", []() {
                return ResourceManager<Scene::Scene>::GetResources();
            });
        type.set_function("AddScene", [](const std::string& filename, Ref<Scene::Scene> scene) {
                return ResourceManager<Scene::Scene>::AddResource(filename, scene);
            });

        auto getOrLoadTerrainOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Terrain::Terrain> {
                return ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoader(filename, Loader::TerrainLoader::LoadTerrain, false);
            },
            [](const std::string& filename, bool loadNodes) -> ResourceHandle<Terrain::Terrain> {
                return ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoader(filename, Loader::TerrainLoader::LoadTerrain, loadNodes);
            }
        );

        auto getOrLoadTerrainAsyncOverload = sol::overload(
            [](const std::string& filename) -> ResourceHandle<Terrain::Terrain> {
                return ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoaderAsync(filename, Loader::TerrainLoader::LoadTerrain, false);
            },
            [](const std::string& filename, bool loadNodes) -> ResourceHandle<Terrain::Terrain> {
                return ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoaderAsync(filename, Loader::TerrainLoader::LoadTerrain, loadNodes);
            }
        );

        type.set_function("GetOrLoadTerrain", getOrLoadTerrainOverload);
        type.set_function("GetOrLoadTerrainAsync", getOrLoadTerrainAsyncOverload);
        type.set_function("GetTerrain", [](const std::string& filename) {
                return ResourceManager<Terrain::Terrain>::GetResource(filename);
            });
        type.set_function("GetTerrains", []() {
                return ResourceManager<Terrain::Terrain>::GetResources();
            });
        type.set_function("AddTerrain", [](const std::string& filename, Ref<Terrain::Terrain> terrain) {
                return ResourceManager<Terrain::Terrain>::AddResource(filename, terrain);
            });

        GenerateAssetLoaderBindings(ns);
        GenerateModelImporterBindings(ns);

    }

}
