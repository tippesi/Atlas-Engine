#include "SplineComponent.h"
#include "common/MatrixDecomposition.h"

#include <glm/gtx/spline.hpp>

namespace Atlas::Scene::Components {

    void SplineComponent::Bake() {

        if (bakeMode == SplineBakeMode::EqualSpacingBake) {
            float timeFraction = controlPoints.size() > 1 ? 1.0f / float(controlPoints.size() - 1) : 0.0f;
            float timePassed = 0.0f;
            for (auto& controlPoint : controlPoints) {
                controlPoint.time = timePassed;
                timePassed += timeFraction;
            }
        }
        else if (bakeMode == SplineBakeMode::DistanceSpacingBake) {

        }
        else {
            std::sort(controlPoints.begin(), controlPoints.end(),
                [&](const SplineControlPoint& ele1, const SplineControlPoint& ele2) {
                    return ele1.time < ele2.time;
                });
        }

        const float epsilon = 0.001f;
        for (size_t i = 0; i < controlPoints.size() - 1; i++) {
            auto& controlPoint = controlPoints[i];
            auto nextPoint = GetInterpolatedFromIndex(int32_t(i), epsilon);

            auto position = vec3(controlPoint.transform[3]);
            auto nextPosition = nextPoint.position;

            controlPoint.tangent = glm::normalize(nextPosition - position);
        }

        // Last point works a bit differently
        if (controlPoints.size() > 1) {
            auto lastIdx = int32_t(controlPoints.size() - 1);
            auto& controlPoint = controlPoints[lastIdx];
            auto prevPoint = GetInterpolatedFromIndex(lastIdx, 1.0f - epsilon);

            auto position = vec3(controlPoint.transform[3]);
            auto prevPosition = prevPoint.position;

            controlPoint.tangent = glm::normalize(position - prevPosition);
        }

    }

    SplinePoint SplineComponent::GetInterpolated(float time, const glm::mat4& parentMatrix) const {

        auto iter = std::lower_bound(controlPoints.begin(), controlPoints.end(), time,
            [](const SplineControlPoint& controlPoint, float time) {
                return controlPoint.time < time;
            });

        if (iter == controlPoints.end() || iter == controlPoints.begin())
            return SplinePoint{};

        auto ele1 = iter - 1;
        auto ele2 = iter;
        
        float timeFract = (time - (*ele1).time) / ((*ele2).time - (*ele1).time);
        int32_t itemIdx = int32_t(iter - controlPoints.begin());

        auto splinePoint = GetInterpolatedFromIndex(itemIdx, timeFract);
        TransformSplinePoint(splinePoint, parentMatrix);

        return splinePoint;

    }

    SplinePoint SplineComponent::GetInterpolatedFromIndex(int32_t idx, float t) const {

        if (controlPoints.empty())
            return SplinePoint {};

        auto interpolateProperty = [type = type]<typename T>(T& p0, T& p1, T& p2, T& p3, float t) -> T {
            if (type == SplineType::Linear) {
                return mix(p1, p2, t);
            }
            else {
                return glm::catmullRom(p0, p1, p2, p3, t);
            }
        };

        auto idx0 = glm::clamp(idx - 1, 0, int32_t(controlPoints.size()) - 1);
        auto idx1 = glm::clamp(idx, 0, int32_t(controlPoints.size()) - 1);
        auto idx2 = glm::clamp(idx + 1, 0, int32_t(controlPoints.size()) - 1);
        auto idx3 = glm::clamp(idx + 2, 0, int32_t(controlPoints.size()) - 1);

        auto decomp0 = Common::MatrixDecomposition(controlPoints[idx0].transform);
        auto decomp1 = Common::MatrixDecomposition(controlPoints[idx1].transform);
        auto decomp2 = Common::MatrixDecomposition(controlPoints[idx2].transform);
        auto decomp3 = Common::MatrixDecomposition(controlPoints[idx3].transform);

        SplinePoint interpolated;

        interpolated.position = type != SplineType::Hermite ? 
            interpolateProperty(decomp0.translation, decomp1.translation,
                decomp2.translation, decomp3.translation, t) : 
            glm::hermite(decomp1.translation, controlPoints[idx1].tangent,
                decomp2.translation, controlPoints[idx2].tangent, t);

        interpolated.scale = interpolateProperty(decomp0.scale, decomp1.scale,
            decomp2.scale, decomp3.scale, t);
        interpolated.rotation = interpolateProperty(decomp0.rotation, decomp1.rotation,
            decomp2.rotation, decomp3.rotation, t);
        interpolated.tangent = interpolateProperty(controlPoints[idx0].tangent, controlPoints[idx1].tangent,
            controlPoints[idx2].tangent, controlPoints[idx3].tangent, t);

        return interpolated;

    }

    void SplineComponent::TransformSplinePoint(SplinePoint& point, const glm::mat4& transform) const {

        point.position = vec3(transform * vec4(point.position, 1.0f));
        point.tangent = glm::normalize(vec3(transform * vec4(point.tangent, 0.0f)));

    }

}