#pragma once

#include "../Entity.h"
#include "../../System.h"

namespace Atlas {

	namespace NewScene {

        class Scene;

		namespace Components {

            class HierarchyComponent {

                friend Scene;

            public:
                HierarchyComponent() = default;
                HierarchyComponent(const HierarchyComponent& that) = default;

                bool root = false;
                std::vector<Entity> entities;

            protected:
                void Update(const TransformComponent& transform, bool parentChanged);

            };

		}

	}

}