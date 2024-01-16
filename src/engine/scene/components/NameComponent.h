#pragma once

#include "../Entity.h"
#include "../../System.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

            class NameComponent {

            public:
                NameComponent() = default;
                NameComponent(const NameComponent& that) = default;
                explicit NameComponent(const std::string& name) : name(name) {}

                std::string name;

            };

		}

	}

}