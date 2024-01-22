#pragma once

#include "../System.h"
#include "../Viewport.h"
#include "../volume/Ray.h"
#include "../terrain/Terrain.h"
#include "events/EventManager.h"

namespace Atlas {

    namespace Tools {

        struct RayIntersection {
            vec3 location = vec3(0.0f);
            vec3 normal = vec3(0.0f, 1.0f, 0.0f);
            float distance = 0.0f;
            bool hasIntersected = false;
        };

        class RayCasting {

        public:
            RayCasting();

            RayIntersection MouseRayIntersection(Viewport *viewport, Camera *camera, 
                Terrain::Terrain *terrain, vec2 mouseOffset = vec2(0.0f));

            RayIntersection MouseRayTerrainIntersection(Viewport *viewport,
                Camera *camera, Terrain::Terrain *terrain, vec2 mouseOffset = vec2(0.0f));

        private:
            RayIntersection BinarySearch(Volume::Ray ray, Terrain::Terrain *terrain, float start,
                                         float finish, int count);

            bool IntersectionInRange(Volume::Ray ray, Terrain::Terrain *terrin, float start, float finish);

            bool IsUnderground(vec3 position, Terrain::Terrain *terrain);

            Volume::Ray CalculateRay(Viewport *viewport, Camera *camera, vec2 mouseOffset);

            void MouseEventHandler(Events::MouseMotionEvent event);

            ivec2 mouseLocation;


        };

    }

}