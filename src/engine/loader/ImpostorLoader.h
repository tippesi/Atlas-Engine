#pragma once

#include "System.h"
#include "mesh/Impostor.h"

namespace Atlas::Loader {

    class ImpostorLoader {

    public:
        static Ref<Mesh::Impostor> LoadImpostor(const std::string& filename, bool binaryJson = true);

        static void SaveImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename, bool binaryJson = true);

    };

}