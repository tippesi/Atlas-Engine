#ifndef AE_TERRAINSTORAGECELL_H
#define AE_TERRAINSTORAGECELL_H

#include "../System.h"
#include "../Material.h"

#include <vector>

namespace Atlas {

	namespace Terrain {

		class TerrainStorage;

		/**
		 * Stores the material information for a terrain node.
		 * Only LoD0 uses a splatmap and the materials
		 */
		class TerrainStorageCell {

		public:
			TerrainStorageCell(TerrainStorage* storage);

			bool IsLoaded();

			void SetMaterial(Material* material, int32_t slot);

			int32_t x = 0;
			int32_t y = 0;
			int32_t LoD = 0;

			int32_t materialIndices[4];

			vec2 position;

			std::vector<float> heightData;

			Texture::Texture2D* heightField = nullptr;
			Texture::Texture2D* normalMap = nullptr;
			Texture::Texture2D* splatMap = nullptr;
			Texture::Texture2D* diffuseMap = nullptr;
			Texture::Texture2D* displacementMap = nullptr;

		private:
			TerrainStorage* const storage;

		};

	}

}

#endif