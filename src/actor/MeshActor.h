#ifndef AE_MESHACTOR_H
#define AE_MESHACTOR_H

#include "System.h"
#include "Actor.h"
#include "../mesh/Mesh.h"

namespace Atlas {

	namespace Actor {

		class MeshActor : public Actor {

		public:
			void Update(Camera camera, float deltaTime,
				mat4 parentTransform, bool parentUpdate) override;

			Mesh::Mesh* mesh;

		protected:
			MeshActor(Mesh::Mesh* mesh) : mesh(mesh) {}

		};


	}

}

#endif