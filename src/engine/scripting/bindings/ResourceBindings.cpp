#include "ResourceBindings.h"

#include "audio/AudioData.h"
#include "Font.h"
#include "mesh/Mesh.h"
#include "scripting/Script.h"

#include "loader/ModelImporter.h"

namespace Atlas::Scripting::Bindings {

    void GenerateResourceManagerBindings(sol::table* ns) {

        GenerateResourceBinding<Audio::AudioData>(ns, "AudioResourceHandle");
        GenerateResourceBinding<Material>(ns, "MaterialResourceHandle");
        GenerateResourceBinding<Texture::Texture2D>(ns, "Texture2DResourceHandle");
        GenerateResourceBinding<Texture::Cubemap>(ns, "CubemapResourceHandle");
        GenerateResourceBinding<Mesh::Mesh>(ns, "MeshResourceHandle");
        GenerateResourceBinding<Scripting::Script>(ns, "ScriptResourceHandle");
        GenerateResourceBinding<Font>(ns, "FontResourceHandle");

        // Text components scale the font by textScale / 16, e.g. ComputeDimensions(text, textScale / 16)
        ns->new_usertype<Font>("Font",
            "ComputeDimensions", [](Font& font, const std::string& text, float scale) {
                vec2 size;
                font.ComputeDimensions(text, scale, &size.x, &size.y);
                return size;
            },
            "lineHeight", &Font::lineHeight
            );

        auto type = ns->create_named("ResourceManager");

        type.set_function("GetOrLoadMesh", [](const std::string& filename) -> ResourceHandle<Mesh::Mesh> { 
                return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(
                    filename, ResourceOrigin::User, Loader::ModelImporter::ImportMesh, false, 2048);
            });

        type.set_function("GetMeshes", []() { 
                return ResourceManager<Mesh::Mesh>::GetResources();
            });

        type.set_function("GetOrLoadScript", [](const std::string& filename) -> ResourceHandle<Scripting::Script> { 
                return ResourceManager<Scripting::Script>::GetOrLoadResourceAsync(
                    filename, ResourceOrigin::User);
            });

        type.set_function("GetScripts", []() { 
                return ResourceManager<Scripting::Script>::GetResources();
            });

        type.set_function("GetOrLoadAudio", [](const std::string& filename) -> ResourceHandle<Audio::AudioData> {
                return ResourceManager<Audio::AudioData>::GetOrLoadResource(filename, ResourceOrigin::User);
            });

        // Same font settings as text components loaded from a scene
        type.set_function("GetOrLoadFont", [](const std::string& filename) -> ResourceHandle<Font> {
                return ResourceManager<Font>::GetOrLoadResourceAsync(filename, ResourceOrigin::User, 32.0f, 7, uint8_t(127));
            });

    }

}