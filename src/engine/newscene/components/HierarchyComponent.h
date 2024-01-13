#pragma once

#include "../Entity.h"
#include "../../System.h"

namespace Atlas {

	namespace NewScene {

		namespace Components {

            class HierarchyComponent {

            public:
                HierarchyComponent() = default;
                HierarchyComponent(const HierarchyComponent& that) = default;

                bool root = false;
                std::vector<Entity> entities;

                mat4 globalMatrix = mat4{ 1.0f };

            };

		}

	}

}