#pragma once

#include "mesh/Mesh.h"

namespace Atlas::Loader {

    class MeshLoader {

    public:
        static Ref<Mesh::Mesh> LoadMesh(const std::string& filename, bool binaryJson = false);

        // Without saveImpostor the caller writes the impostor itself, e.g. ImpostorLoader::WriteImpostor in a job
        static void SaveMesh(const Ref<Mesh::Mesh>& mesh, const std::string& filename,
            bool binaryJson = false, bool formatJson = false, bool saveImpostor = true);

    };

}