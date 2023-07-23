#include "VegetationMesh.h"

namespace Atlas {

    namespace Mesh {

        VegetationMesh::VegetationMesh(const VegetationMesh& that) {



        }

        VegetationMesh::VegetationMesh(ResourceHandle<MeshData> meshData,
            MeshMobility mobility) : Mesh(data, mobility) {



        }

    }

}


