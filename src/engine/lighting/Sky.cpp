#include "Sky.h"

#include "../common/MathHelper.h"

namespace Atlas {

    namespace Lighting {

        Ref<EnvironmentProbe> Sky::GetProbe() {

            // Prioritize user loaded cubemaps
            if (probe) return probe;
            if (atmosphere) return atmosphere->probe;

            return nullptr;

        }

        vec3 Sky::GetDirectLightTransmittance(vec3 position, vec3 lightDirection) const {

            if (!atmosphere || atmosphere->directLight <= 0.0f)
                return vec3(1.0f);

            // Like the transmittance LUT: the optical depth along the ray towards the light up to the top of the
            // atmosphere, the heights over the surface the atmosphere renderer uses
            auto direction = -glm::normalize(lightDirection);
            auto origin = position - planetCenter;
            auto surfaceOffset = -(planetCenter.y + planetRadius);
            auto atmosphereRadius = planetRadius + atmosphere->height;

            auto b = glm::dot(origin, direction);
            auto discriminant = b * b - (glm::dot(origin, origin) - atmosphereRadius * atmosphereRadius);
            if (discriminant <= 0.0f)
                return vec3(1.0f);
            auto length = -b + sqrtf(discriminant);
            if (length <= 0.0f)
                return vec3(1.0f);

            // Below the horizon the planet stands between the position and the light
            auto planet = b * b - (glm::dot(origin, origin) - planetRadius * planetRadius);
            vec3 transmittance(0.0f);
            if (planet < 0.0f || -b - sqrtf(planet) < 0.0f) {
                const int32_t steps = 64;
                float rayleigh = 0.0f, mie = 0.0f;
                for (int32_t i = 0; i < steps; i++) {
                    auto t = (float(i) + 0.5f) / float(steps) * length;
                    auto height = glm::max(glm::length(origin + direction * t) - planetRadius - surfaceOffset, 0.0f);
                    rayleigh += expf(-height / atmosphere->rayleighHeightScale);
                    mie += expf(-height / atmosphere->mieHeightScale);
                }
                auto step = length / float(steps);
                transmittance = glm::exp(-(atmosphere->rayleighScatteringCoeff * rayleigh * step + atmosphere->mieScatteringCoeff * mie * step));
            }

            return glm::mix(vec3(1.0f), transmittance, Helper::Math::Saturate(atmosphere->directLight));

        }

    }

}