#ifndef AE_TERRAINSTORAGECELL_H
#define AE_TERRAINSTORAGECELL_H

#include "../System.h"
#include "../texture/Texture2D.h"

#include <vector>

namespace Atlas {

	namespace Terrain {

		/**
		 * Stores the material information for a terrain node.
		 * Only LoD0 uses a splatmap and the materials
		 */
		class TerrainStorageCell {

		public:
			TerrainStorageCell();

			bool IsLoaded();

			int32_t x;
			int32_t y;
			int32_t LoD;

			vec2 position;

			std::vector<float> heightData;

			Texture::Texture2D* heightField;
			Texture::Texture2D* normalMap;
			Texture::Texture2D* diffuseMap;
			Texture::Texture2D* displacementMap;

		};

	}

}

#endif