#include "VegetationMesh.h"

namespace Atlas {

	namespace Mesh {

		VegetationMesh::VegetationMesh(const VegetationMesh& that) {



		}

		VegetationMesh::VegetationMesh(MeshData data, int32_t mobility) : Mesh(data, mobility) {



		}

		VegetationMesh::VegetationMesh(const std::string& filename, bool forceTangents, int32_t maxTextureResolution, int32_t mobility) :
			Mesh(filename, forceTangents, maxTextureResolution, mobility) {



		}

	}

}


