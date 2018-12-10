#include "RayCasting.h"

RayCasting::RayCasting() {

    auto mouseEventHandler = std::bind(&RayCasting::MouseEventHandler, this, std::placeholders::_1);
    EngineEventHandler::MouseMotionEventDelegate.Subscribe(mouseEventHandler);

}

RayIntersection RayCasting::MouseRayIntersection(Viewport* viewport, Camera* camera, Terrain* terrain) {

	RayIntersection intersection;

	auto ray = CalculateRay(viewport, camera);

	return intersection;

}

RayIntersection RayCasting::BinarySearch(Ray ray, Terrain* terrain, float start, 
	float finish, int count) {

	float half = start + (finish - start) / 2.0f;

	if (count == 0) {
		auto position = ray.origin + ray.direction * half;

	}

	if (IntersectionInRange(ray, terrain, start, half)) {
		return BinarySearch(ray, terrain, start, half, count - 1);
	}
	else {
		return BinarySearch(ray, terrain, half, finish, count - 1);
	}

}

bool RayCasting::IntersectionInRange(Ray ray, Terrain* terrain, float start, float finish) {

	auto startPosition = ray.origin + ray.direction * start;
	auto finishPosition = ray.origin + ray.direction * finish;

	if (!IsUnderground(startPosition, terrain) && IsUnderground(finishPosition, terrain)) {
		return true;
	}

	return false;

}

bool RayCasting::IsUnderground(vec3 position, Terrain* terrain) {

	//float height = 

	return true;

}

RayCasting::Ray RayCasting::CalculateRay(Viewport *viewport, Camera *camera) {

    Ray ray;

    auto nearPoint = viewport->Unproject(vec3(mouseLocation.x, mouseLocation.y, 0.0f), camera);
    auto farPoint = viewport->Unproject(vec3(mouseLocation.x, mouseLocation.y, 1.0f), camera);

    ray.direction = glm::normalize(farPoint - nearPoint);
    ray.origin = nearPoint;

    return ray;

}

void RayCasting::MouseEventHandler(EngineMouseMotionEvent event) {

    mouseLocation = glm::vec2((float)event.x, (float)event.y);

}