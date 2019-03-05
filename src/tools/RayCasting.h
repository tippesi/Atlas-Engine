#ifndef AE_RAYCASTING_H
#define AE_RAYCASTING_H

#include "../System.h"
#include "../Viewport.h"
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
			typedef struct Ray {
				vec3 origin;
				vec3 direction;
			} Ray;

			RayIntersection BinarySearch(Ray ray, Terrain::Terrain *terrain, float start,
										 float finish, int count);

			bool IntersectionInRange(Ray ray, Terrain::Terrain *terrin, float start, float finish);

			bool IsUnderground(vec3 position, Terrain::Terrain *terrain);

			Ray CalculateRay(Viewport *viewport, Camera *camera);

			void MouseEventHandler(Events::MouseMotionEvent event);

			ivec2 mouseLocation;


		};

	}

}

#endif