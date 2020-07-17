#ifndef AE_CUBEMAPARRAY_H
#define AE_CUBEMAPARRAY_H

#include "../System.h"

#include "Texture.h"
#include "Cubemap.h"

namespace Atlas {

	namespace Texture {

		class CubemapArray : public Texture {

		public:
			/**
			 * Constructs a CubemapArray object.
			 */
			CubemapArray() = default;

			/**
			 * Constructs a CubemapArray object.
			 * @param that Another CubemapArray object.
			 */
			CubemapArray(const CubemapArray& that);

			/**
			 * Construct a CubemapArray object.
			 * @param width The width of the texture.
			 * @param height The height of the texture.
			 * @param depth The number of cubemaps.
			 * @param sizedFormat The sized texture format. See {@link TextureFormat.h} for more.
			 * @param wrapping The wrapping of the texture. Controls texture border behaviour.
			 * @param filtering The filtering of the texture.
			 * @param anisotropicFiltering Whether or not anisotropic filtering is used.
			 * @param generateMipMaps Whether or not mipmap can be used. Generate using GenerateMipmap()
			 */
			CubemapArray(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
				int32_t wrapping = GL_CLAMP_TO_EDGE, int32_t filtering = GL_NEAREST,
				bool anisotropicFiltering = false, bool generateMipMaps = false);

			/**
			 * Copies the data from another texture to the texture object.
			 * @param that Another texture.
			 * @return A reference to the texture.
			 * @note The graphics API object will be changed.
			 */
			CubemapArray& operator=(const CubemapArray& that);

			/**
			 * Sets the data of a cubemap in the array.
			 * @param that Another cubemap.
			 * @param depth The index of the cubemap to be set.
			 */
			void SetData(const Cubemap& that, int32_t depth);

		protected:
			void ReserveStorage(int32_t mipCount);

		};

	}

}

#endif