#ifndef AE_MATERIALLOADER_H
#define AE_MATERIALLOADER_H

#include "../System.h"
#include "../Material.h"

#include <string>

namespace Atlas {

    namespace Loader {

        class MaterialLoader {

		public:
            static Material* LoadMaterial(std::string filename, int32_t mapResolution = 0);

            static void SaveMaterial(Material* material, std::string filename);

		private:
			static Material* LoadMaterialValues(std::ifstream& stream, int32_t& textureCount);

			static std::string WriteVector(std::string prefix, vec3 vector);

			static vec3 ReadVector(std::string line);

			static float ReadFloat(std::string line);

			static std::string ReadFilePath(std::string line, std::string materialDirectory);

        };

    }

}


#endif