#pragma once

#include "../Entity.h"
#include "../../System.h"

namespace Atlas::Scene::Components {

    struct SplineControlPoint {
        glm::mat4 transform{1.0f};

        vec3 tangent{1.0f};

        float time = 0.0f;
    };

    struct SplinePoint {
        vec3 position{1.0f};
        vec3 rotation{1.0f};
        vec3 scale{1.0f};

        vec3 tangent{1.0f};

        float time = 0.0f;
    };


    class SplineComponent {

    public:
        enum class SplineType {
            Linear = 0,
            CatmullRom,
            Hermite
        };

        SplineComponent() = default;
        SplineComponent(const SplineComponent& that) = default;
        explicit SplineComponent(const SplineType type) : type(type) {}

        void Bake();

        SplinePoint GetInterpolated(float time, const glm::mat4& parentMatrix);

        SplinePoint GetInterpolated(int32_t idx, float t) const;

        SplineType type = SplineType::Linear;
        std::vector<SplineControlPoint> controlPoints;
        std::vector<SplinePoint> bakedPoints;

    private:
        void TransformSplinePoint(SplinePoint& point, const glm::mat4& transform);

    };

}