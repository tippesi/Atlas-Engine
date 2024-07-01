#include "UtilityBindings.h"

#include "Clock.h"
#include "Log.h"
#include "common/MatrixDecomposition.h"

#include "volume/AABB.h"
#include "volume/Frustum.h"

namespace Atlas::Scripting::Bindings {

    void GenerateUtilityBindings(sol::table* ns) {

        auto log = ns->new_usertype<Log>("Log");
        // Set it manually here to avoid having a function that require the optional argument to log
        log.set_function("Message", [](const std::string& msg) { Log::Message(msg); });
        log.set_function("Warning", [](const std::string& warn) { Log::Warning(warn); });
        log.set_function("Error", [](const std::string& err) { Log::Error(err); });

        ns->new_usertype<Clock>("Clock",
            "Get", &Clock::Get,
            "GetDelta", &Clock::GetDelta,
            "GetAverage", &Clock::GetAverage);

        ns->new_usertype<Common::MatrixDecomposition>("MatrixDecomposition",
            sol::call_constructor,
            sol::constructors<Common::MatrixDecomposition(glm::mat4)>(),
            "Compose", &Common::MatrixDecomposition::Compose,
            "Decompose", &Common::MatrixDecomposition::Decompose,
            "translation", &Common::MatrixDecomposition::translation,
            "rotation", &Common::MatrixDecomposition::rotation,
            "scale", &Common::MatrixDecomposition::scale,
            "perspective", &Common::MatrixDecomposition::perspective,
            "skew", &Common::MatrixDecomposition::skew,
            "quaternion", &Common::MatrixDecomposition::quaternion);

    }

    void GenerateVolumeBindings(sol::table* ns) {

        auto isInsideAABBOverload = sol::overload(
            [](Volume::AABB& aabb0, const Volume::AABB& aabb1) { return aabb0.IsInside(aabb1); },
            [](Volume::AABB& aabb, const vec3& vec) { return aabb.IsInside(vec); }
        );

        auto growAABBOverload = sol::overload(
            [](Volume::AABB& aabb0, const Volume::AABB& aabb1) { return aabb0.Grow(aabb1); },
            [](Volume::AABB& aabb, const vec3& vec) { return aabb.Grow(vec); }
        );

        ns->new_usertype<Volume::AABB>("AABB",
            sol::call_constructor,
            sol::constructors<Volume::AABB(), Volume::AABB(glm::vec3, glm::vec3)>(),
            "Intersects", &Volume::AABB::Intersects,
            "IsInside", isInsideAABBOverload,
            "Transform", &Volume::AABB::Transform,
            "Translate", &Volume::AABB::Translate,
            "Scale", &Volume::AABB::Scale,
            "Grow", growAABBOverload,
            "Intersect", &Volume::AABB::Intersect,
            "GetSurfaceArea", &Volume::AABB::GetSurfaceArea,
            "GetSize", &Volume::AABB::GetSize,
            "GetDistance", &Volume::AABB::GetDistance,
            "GetCorners", &Volume::AABB::GetCorners,
            "min", &Volume::AABB::min,
            "max", &Volume::AABB::max
            );

        auto resizeFrustumOverload = sol::overload(
            [](Volume::Frustum& frustum, const std::vector<vec3>& corners) { frustum.Resize(corners); },
            [](Volume::Frustum& frustum, const mat4& matrix) { frustum.Resize(matrix); }
        );

        ns->new_usertype<Volume::Frustum>("Frustum",
            sol::call_constructor,
            sol::constructors<Volume::Frustum(), Volume::Frustum(const std::vector<vec3>&), Volume::Frustum(glm::mat4)>(),
            "Resize", resizeFrustumOverload,
            "Intersects", &Volume::Frustum::Intersects,
            "IsInside", &Volume::Frustum::IsInside,
            "GetPlanes", &Volume::Frustum::GetPlanes,
            "GetCorners", &Volume::Frustum::GetCorners
            );

    }

}