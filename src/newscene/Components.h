#ifndef AE_COMPONENTS_H
#define AE_COMPONENTS_H

#include "../System.h"

namespace Atlas {

    namespace NewScene {

        class NameComponent {

        public:
            NameComponent() = default;
            NameComponent(const std::string& name) : name(name) {}
            NameComponent(std::string name) : name(name) {}
            NameComponent(const NameComponent& that) = default;

            std::string name;

        };

        class TransformComponent {

        public:
            TransformComponent() = default;
            TransformComponent(mat4 matrix) : matrix(matrix) {}
            TransformComponent(const TransformComponent& that) = default;

            glm::mat4 matrix;

        };

        class MeshComponent {

        public:


        };

    }

}

#endif