#include "ResourceBindings.h"

#include "audio/AudioData.h"
#include "mesh/Mesh.h"
#include "scripting/Script.h"

#include "loader/ModelLoader.h"

namespace Atlas::Scripting::Bindings {

    void GenerateResourceManagerBindings(sol::table* ns) {

        GenerateResourceBinding<Audio::AudioData>(ns, "AudioResourceHandle");
        GenerateResourceBinding<Mesh::Mesh>(ns, "MeshResourceHandle");
        GenerateResourceBinding<Scripting::Script>(ns, "ScriptResourceHandle");

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

    }

}