#ifndef AE_VEGETATIONMESH_H
#define AE_VEGETATIONMESH_H

#include "../System.h"
#include "Mesh.h"

namespace Atlas {

	namespace Mesh {

		class VegetationMesh : public Mesh {

		public:
			VegetationMesh() = default;

			VegetationMesh(const VegetationMesh& that);

			/**
			 *
			 * @param filename
			 * @param mobility
			 */
			explicit VegetationMesh(MeshData data, int32_t mobility = AE_STATIONARY_MESH);

			/**
			 *
			 * @param filename
			 * @param forceTangents
			 * @param mobility
			 */
			explicit VegetationMesh(const std::string& filename, bool forceTangens = false,
				int32_t maxTextureResolution = 4096, int32_t mobility = AE_STATIONARY_MESH);

		private:


		};


	}

}


#endif