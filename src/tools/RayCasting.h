#ifndef AE_RAYCASTING_H
#define AE_RAYCASTING_H

#include "../System.h"
#include "../Viewport.h"
#include "../volume/Ray.h"
#include "actor/MeshActor.h"
#include "../terrain/Terrain.h"
#include "events/EventManager.h"

namespace Atlas {

	namespace Tools {

		typedef struct RayIntersection {
			vec3 location;
			float distance;
			bool hasIntersected;
		} RayIntersection;

		class RayCasting {

		public:
			RayCasting();

			RayIntersection MouseRayIntersection(Viewport *viewport, Camera *camera, Terrain::Terrain *terrain);

			RayIntersection MouseRayTerrainIntersection(Viewport *viewport, Camera *camera, Terrain::Terrain *terrain);

		private:
			RayIntersection BinarySearch(Volume::Ray ray, Terrain::Terrain *terrain, float start,
										 float finish, int count);

			bool IntersectionInRange(Volume::Ray ray, Terrain::Terrain *terrin, float start, float finish);

			bool IsUnderground(vec3 position, Terrain::Terrain *terrain);

			Volume::Ray CalculateRay(Viewport *viewport, Camera *camera);

			void MouseEventHandler(Events::MouseMotionEvent event);

			ivec2 mouseLocation;


		};

	}

}

#endif