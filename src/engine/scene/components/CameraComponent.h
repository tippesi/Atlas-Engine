#pragma once

#include "../Entity.h"
#include "../../System.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

            class CameraComponent {

            public:
                CameraComponent() = default;
                CameraComponent(const CameraComponent& that) = default;

                bool main = true;

                Ref<Camera> camera;

            };

		}

	}

}