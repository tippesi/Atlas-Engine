#ifndef AE_IMPOSTORTOOL_H
#define AE_IMPOSTORTOOL_H

#include "../System.h"
#include "../mesh/Impostor.h"
#include "../mesh/Mesh.h"

namespace Atlas {

	namespace Tools {

		class ImpostorTool {

		public:
			static Mesh::Impostor* GenerateImpostor(Mesh::Mesh* mesh, int32_t resolution);

		};

	}

}

#endif