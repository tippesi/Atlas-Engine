#include "RayCasting.h"

namespace Atlas {

    namespace Tools {

        RayCasting::RayCasting() {

            auto mouseEventHandler = std::bind(&RayCasting::MouseEventHandler, this, std::placeholders::_1);
            Events::EventManager::MouseMotionEventDelegate.Subscribe(mouseEventHandler);

        }

        RayIntersection RayCasting::MouseRayIntersection(Ref<Viewport> viewport, Ref<Terrain::Terrain> terrain,
            const CameraComponent& camera, vec2 mouseOffset) {

            auto intersection = MouseRayTerrainIntersection(viewport, 
                terrain, camera, mouseOffset);

            return intersection;

        }

        RayIntersection RayCasting::MouseRayTerrainIntersection(Ref<Viewport> viewport, Ref<Terrain::Terrain> terrain,
            const CameraComponent& camera, vec2 mouseOffset) {

            const float linearStepLength = 1.0f;

            auto ray = CalculateRay(viewport, camera, mouseOffset);

            auto distance = linearStepLength;

            vec3 position = ray.origin;
            vec3 nextPosition;

            while (distance < camera.farPlane) {
                nextPosition = ray.origin + ray.direction * distance;
                if (!IsUnderground(position, terrain) && IsUnderground(nextPosition, terrain)) {
                    return BinarySearch(ray, terrain, distance - linearStepLength, distance, 10);
                }
                position = nextPosition;
                distance += linearStepLength;
            }

            RayIntersection intersection;
            intersection.hasIntersected = false;
            intersection.location = vec3(0.0f);
            intersection.distance = 0.0f;
            return intersection;

        }

        RayIntersection RayCasting::BinarySearch(Volume::Ray ray, Ref<Terrain::Terrain>& terrain, float start,
                float finish, int count) {

            float half = start + (finish - start) / 2.0f;

            if (count == 0) {
                auto position = ray.origin + ray.direction * half;
                RayIntersection intersection;
                intersection.location = position;
                intersection.distance = half;
                intersection.hasIntersected = true;
                return intersection;
            }

            if (IntersectionInRange(ray, terrain, start, half)) {
                return BinarySearch(ray, terrain, start, half, count - 1);
            }
            else {
                return BinarySearch(ray, terrain, half, finish, count - 1);
            }

        }

        bool RayCasting::IntersectionInRange(Volume::Ray ray, Ref<Terrain::Terrain>& terrain, float start, float finish) {

            auto startPosition = ray.origin + ray.direction * start;
            auto finishPosition = ray.origin + ray.direction * finish;

            if (!IsUnderground(startPosition, terrain) && IsUnderground(finishPosition, terrain)) {
                return true;
            }

            return false;

        }

        bool RayCasting::IsUnderground(vec3 position, Ref<Terrain::Terrain>& terrain) {

            float height = terrain->GetHeight(position.x, position.z);

            return (height > position.y);

        }

        Volume::Ray RayCasting::CalculateRay(const Ref<Viewport>& viewport,
            const CameraComponent& camera, vec2 mouseOffset) {

            Volume::Ray ray;

            auto location = vec2(mouseLocation) - mouseOffset;

            auto nearPoint = viewport->Unproject(vec3(location, 0.0f), camera);
            auto farPoint = viewport->Unproject(vec3(location, 1.0f), camera);

            ray.direction = glm::normalize(farPoint - nearPoint);
            ray.origin = nearPoint;

            return ray;

        }

        void RayCasting::MouseEventHandler(Events::MouseMotionEvent event) {

            mouseLocation = glm::vec2((float)event.x, (float)event.y);

        }

    }

}