#include "MaterialLoader.h"
#include "AssetLoader.h"

namespace Atlas {

    namespace Loader {

        Material* MaterialLoader::LoadMaterial(std::string filename) {

            auto stream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!stream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to load material %s", filename.c_str());
#endif
                throw AtlasException("Material couldn't be loaded");
            }

			std::string header;

			std::getline(stream, header);

			if (header.compare(0, 4, "AEM ") != 0) {
				throw AtlasException("File isn't a material file");
			}



            stream.close();

			return nullptr;

        }

        void MaterialLoader::SaveMaterial(Atlas::Material *material, std::string filename) {

            auto stream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!stream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to save material %s", filename.c_str());
#endif
                throw AtlasException("Material couldn't be loaded");
            }

			std::string header;

			header.append("AEM ");

			stream << header;



            stream.close();

        }

    }

}