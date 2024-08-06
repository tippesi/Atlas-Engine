#pragma once

#include "mesh/Mesh.h"

namespace Atlas::Loader {

    class MeshLoader {

    public:
        static Ref<Mesh::Mesh> LoadMesh(const std::string& filename, bool binaryJson = false);

        static void SaveMesh(const Ref<Mesh::Mesh>& mesh, const std::string& filename, 
            bool binaryJson = false, bool formatJson = false);

    };

}