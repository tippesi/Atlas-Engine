#include <glm.hpp>

#include <glm/gtc/constants.hpp>

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

        template<typename T>
        constexpr inline float Sum(T t) {

            return glm::dot(t, T(1.0f));

        }

        template<typename T>
        constexpr inline float Avg(T t) {

            return glm::dot(t, T(1.0f)) / Sum(T(1.0f));

        }

        inline glm::vec3 Cartesian(glm::vec2 spherical) {

            return glm::vec3(
                sinf(spherical.x) * cosf(spherical.y),
                sinf(spherical.x) * sinf(spherical.y),
                cosf(spherical.x)
            );

        }

        inline glm::vec2 Spherical(glm::vec3 cartesian) {

            auto theta = acosf(cartesian.z);
            auto phi = atan2f(cartesian.y, cartesian.x);
            phi = phi < 0.0f ? phi + glm::two_pi<float>() : phi;

            return glm::vec2(theta, phi);

        }

    }

}