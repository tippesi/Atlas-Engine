#pragma once

#include "../../System.h"

namespace Atlas::Scene::Components {
    
    class ClutterComponent {

	public:
        ClutterComponent() = default;
		ClutterComponent(const ClutterComponent& that) = default;
		ClutterComponent(const mat4x3& matrix);

        mat4x3 matrix;

	};


}