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
			explicit VegetationMesh(MeshData data, MeshMobility mobility = MeshMobility::Stationary);

		private:


		};


	}

}


#endif