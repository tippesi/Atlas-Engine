#include "SplineComponent.h"
#include "common/MatrixDecomposition.h"

#include <glm/gtx/spline.hpp>

namespace Atlas::Scene::Components {

    SplinePoint SplineComponent::GetInterpolated(int32_t idx, float t) const {

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

        return interpolated;

    }

    void SplineComponent::TransformSplinePoint(SplinePoint& point, const glm::mat4& transform) {



    }

}