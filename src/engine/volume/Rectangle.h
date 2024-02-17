#pragma once

#include "System.h"

namespace Atlas::Volume {

    class Rectangle {

    public:
        Rectangle(vec3 point, vec3 s0, vec3 s1) : point(point), s0(s0), s1(s1) {}

        vec3 GetNormal() const { return glm::normalize(glm::cross(s1, s0)); }

        vec3 point;
        vec3 s0;
        vec3 s1;

    };

}