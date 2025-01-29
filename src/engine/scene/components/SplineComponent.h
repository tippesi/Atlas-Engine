#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "common/MatrixDecomposition.h"

namespace Atlas::Scene::Components {

    struct SplineControlPoint {
        SplineControlPoint() {}

        SplineControlPoint(glm::mat4 matrix, float time = 0.0f) : transform(matrix), time(time) {
            transform = matrix;
            matrixDecomposition = Common::MatrixDecomposition(matrix);
        }

        glm::mat4 transform{1.0f};
        Common::MatrixDecomposition matrixDecomposition;

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

    enum class SplineType {
        Linear = 0,
        CatmullRom,
        Hermite
    };

    enum class SplineBakeMode {
        NoBake = 0,
        EqualSpacingBake,
        DistanceSpacingBake
    };

    class SplineComponent {

    public:
        SplineComponent() = default;
        SplineComponent(const SplineComponent& that) = default;
        explicit SplineComponent(const SplineType type) : type(type) {}

        void Bake();

        SplinePoint GetInterpolated(float time, const glm::mat4& parentMatrix) const;

        SplinePoint GetInterpolatedFromIndex(int32_t idx, float t) const;

        SplineType type = SplineType::Linear;
        SplineBakeMode bakeMode = SplineBakeMode::EqualSpacingBake;

        std::vector<SplineControlPoint> controlPoints;
        std::vector<SplinePoint> bakedPoints;

    private:
        void TransformSplinePoint(SplinePoint& point, const glm::mat4& transform) const;

    };

}