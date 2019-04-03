//
// Created by tippes on 01.04.19.
//

#ifndef AE_MATERIALLOADER_H
#define AE_MATERIALLOADER_H

#include "../System.h"
#include "../Material.h"

#include <string>

namespace Atlas {

    namespace Loader {

        class MaterialLoader {

            static Material* LoadMaterial(std::string filename);

            static void SaveMaterial(Material* material, std::string filename);

        };

    }

}


#endif