#pragma once

#include "../System.h"

#include "Entity.h"

#include <functional>

namespace Atlas {

    namespace ECS {

        enum class Topic {
            ComponentErase = 0,
            ComponentEmplace = 1
        };

        template<typename Comp>
        struct Subscriber {
            size_t ID = 0;
            std::function<void(const Entity, Comp&)> function;
        };

    }

}