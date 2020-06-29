#include "CubemapArray.h"

namespace Atlas {

	namespace Texture {

		CubemapArray::CubemapArray(const CubemapArray& that) {

			DeepCopy(that);

		}

		CubemapArray::CubemapArray(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
			int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

			this->width = width;
			this->height = height;
			this->layers = depth * 6;

			Generate(GL_TEXTURE_CUBE_MAP_ARRAY, sizedFormat, wrapping, filtering,
				anisotropicFiltering, generateMipMaps);

		}

		CubemapArray& CubemapArray::operator=(const CubemapArray& that) {

			if (this != &that) {

				Texture::operator=(that);

			}

			return *this;

		}

		void CubemapArray::SetData(const Cubemap& that, int32_t depth) {

			depth *= 6;
			Texture::Copy(that, 0, 0, 0, 0, 0, depth, width, height, 6);

		}

		void CubemapArray::ReserveStorage(int32_t mipCount) {

			glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mipCount, 
				sizedFormat, width, height, layers);

		}

	}

}