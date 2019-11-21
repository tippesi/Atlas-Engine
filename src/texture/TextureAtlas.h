#ifndef AE_TEXTUREATLAS_H
#define AE_TEXTUREATLAS_H

#include "../System.h"
#include "Texture2D.h"
#include "Texture2DArray.h"

#include <map>

namespace Atlas {

	namespace Texture {

		class TextureAtlas {

		public:
			/**
			 * Constructs a TextureAtlas object.
			 * @note Texture atlases are only available as AE_RGBA8.
			 */
			TextureAtlas() {}

			/**
			 * Constructs a TextureAtlas object.
			 * @param that Another TextureAtlas object.
			 * @note Texture atlases are only available as AE_RGBA8.
			 */
			TextureAtlas(const TextureAtlas& that);

			/**
			 * Constructs a TextureAtlas object.
			 * @param that Another TextureAtlas object.
			 * @note Texture atlases are only available as AE_RGBA8.
			 */
			TextureAtlas(std::vector<Texture2D*>& textures, int32_t padding = 0);

			/**
			 * Copies the data from another texture atlas to the texture atlas object.
			 * @param that Another texture atlas.
			 * @return A reference to the texture atlas.
			 * @note The graphics API objects will be changed.
			 */
			TextureAtlas& operator=(const TextureAtlas& that);

			void Update(std::vector<Texture2D*>& textures);

			struct Slice {

				int32_t layer;
				ivec2 offset;
				ivec2 size;

			};

			Texture2DArray texture;
			std::map<Texture2D*, Slice> slices;

		private:
			int32_t padding = 0;

		};

	}

}

#endif