#ifndef AE_IMPOSTORTOOL_H
#define AE_IMPOSTORTOOL_H

#include "../System.h"
#include "../mesh/Impostor.h"
#include "../mesh/Mesh.h"
#include "../Camera.h"

namespace Atlas {

	namespace Tools {

		class ImpostorTool {

		public:
			static Mesh::Impostor* GenerateImpostor(Mesh::Mesh* mesh, Camera* camera,
				int32_t views, int32_t resolution);

		};

	}

}

#endif