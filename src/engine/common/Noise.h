#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"
#include "Image.h"
#include "RandomHelper.h"

#include <perlin noise/PerlinNoise.h>
#include <algorithm>

#include <glm/gtc/noise.hpp>

namespace Atlas::Common {

    inline vec3 Random3D(vec3 p) {
        vec3 q = vec3(glm::dot(p, vec3(127.1f, 311.7f, 74.7f)),
            dot(p, vec3(269.5f, 183.3f, 246.1f)),
            dot(p, vec3(113.5f, 271.9f, 124.6f)));
        return glm::fract(glm::sin(q) * 43758.5453f);
    }

    inline float Worley(vec3 pos, float scale, float seed) {

        pos *= scale;
        pos += 0.5;

        vec3 base = glm::floor(pos);
        vec3 frac = glm::fract(pos);

        float dist = 1.0;
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    vec3 cell = vec3(x, y, z);
                    vec3 r = cell - frac + Random3D(glm::mod(base + cell, scale) + seed);
                    float d = glm::dot(r, r);

                    dist = glm::min(dist, d);
                }
            }
        }

        return glm::clamp(dist, 0.0f, 1.0f);

    }

}