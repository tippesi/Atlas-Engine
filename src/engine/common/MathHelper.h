#pragma once

#include "../System.h"

#include <glm/gtc/constants.hpp>

#include <span>

namespace Helper {

    namespace Math {

        template<typename T>
        constexpr inline T Saturate(T t) {

            return glm::clamp(t, T(0.0f), T(1.0f));

        }

        template<typename T>
        constexpr inline T Sqr(T t) {

            return t * t;

        }

        // Zero-length vectors have no direction.
        template<typename T>
        inline T NormalizeOrZero(T direction) {

            auto length = glm::length(direction);
            return length > 0.0f ? direction / length : T(0.0f);

        }

        template<typename T>
        constexpr inline float Sum(T t) {

            return glm::dot(t, T(1.0f));

        }

        template<typename T>
        constexpr inline float Avg(T t) {

            return glm::dot(t, T(1.0f)) / Sum(T(1.0f));

        }

        inline Atlas::vec3 Cartesian(Atlas::vec2 spherical) {

            return Atlas::vec3(
                sinf(spherical.x) * cosf(spherical.y),
                sinf(spherical.x) * sinf(spherical.y),
                cosf(spherical.x)
            );

        }

        inline Atlas::vec2 Spherical(Atlas::vec3 cartesian) {

            auto theta = acosf(cartesian.z);
            auto phi = atan2f(cartesian.y, cartesian.x);
            phi = phi < 0.0f ? phi + glm::two_pi<float>() : phi;

            return Atlas::vec2(theta, phi);

        }

        inline float DistanceToSegment(Atlas::vec2 point, Atlas::vec2 a, Atlas::vec2 b) {

            auto ab = b - a;
            auto lengthSquared = glm::dot(ab, ab);
            auto t = lengthSquared > 0.0f ? Saturate(glm::dot(point - a, ab) / lengthSquared) : 0.0f;
            return glm::distance(point, a + t * ab);

        }

        // Even-odd rule, the polygon is closed between its last and first point
        inline bool IsInsidePolygon(Atlas::vec2 point, std::span<const Atlas::vec2> polygon) {

            bool inside = false;
            for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
                const auto& a = polygon[i];
                const auto& b = polygon[j];
                if ((a.y > point.y) != (b.y > point.y) &&
                    point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                    inside = !inside;
            }

            return inside;

        }

    }

}
