#pragma once

#include "../Entity.h"

#include "../components/HierarchyComponent.h"
#include "../components/NameComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Prefabs {

            class Group : public Entity {

            public:
                Group(ECS::Entity entity, ECS::EntityManager* manager, const std::string& name)
                    : Entity(entity, manager) {

                    AddComponent<HierarchyComponent>();
                    AddComponent<NameComponent>(name);

                }

            };

        }

    }

}