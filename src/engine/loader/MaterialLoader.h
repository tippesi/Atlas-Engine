#pragma once

#include "../System.h"
#include "../Material.h"

#include <string>

namespace Atlas::Loader {

    class MaterialLoader {

    public:
        static Ref<Material> LoadMaterial(const std::string& filename, bool binaryJson = false);

        static void SaveMaterial(const Ref<Material>& material, const std::string& filename,
            bool binaryJson = false, bool formatJson = false);

    };

}