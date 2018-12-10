#ifndef RAYCASTING_H
#define RAYCASTING_H

#include "../System.h"
#include "../Viewport.h"
#include "../mesh/Actor.h"
#include "../terrain/Terrain.h"
#include "../events/EngineEventHandler.h"

typedef struct RayIntersection {
	vec3 location;
	float distance;
	bool hasIntersected;
}RayIntersection;

class RayCasting {

public:
    RayCasting();

	RayIntersection MouseRayIntersection(Viewport* viewport, Camera* camera, Terrain* terrain);

private:
    typedef struct Ray {
        vec3 origin;
        vec3 direction;
    }Ray;

	RayIntersection BinarySearch(Ray ray, Terrain* terrain, float start, 
		float finish, int count);

	bool IntersectionInRange(Ray ray, Terrain* terrin, float start, float finish);

	bool IsUnderground(vec3 position, Terrain* terrain);

    Ray CalculateRay(Viewport* viewport, Camera* camera);

    void MouseEventHandler(EngineMouseMotionEvent event);

    glm::ivec2 mouseLocation;


};

#endif