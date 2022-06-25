#ifndef AE_COMPONENTS_H
#define AE_COMPONENTS_H

#include "Entity.h"
#include "../System.h"

#include "../mesh/Mesh.h"
#include "../common/MatrixDecomposition.h"

namespace Atlas {

    namespace NewScene {

        class NameComponent {

        public:
            NameComponent() = default;
            NameComponent(const NameComponent& that) = default;
            explicit NameComponent(const std::string& name) : name(name) {}

            std::string name;

        };

        class TransformComponent {

        public:
            TransformComponent() = default;
            TransformComponent(const TransformComponent& that) = default;
            explicit TransformComponent(mat4 matrix) : matrix(matrix) {}

            void Translate(glm::vec3 translation);
            void Rotate(glm::vec3 rotation);
            void Scale(glm::vec3 scale);

            Common::MatrixDecomposition Decompose() const;
            void Compose(Common::MatrixDecomposition composition);

            glm::mat4 matrix;

        };

        class MeshComponent {

        public:
            MeshComponent() = default;
            MeshComponent(const MeshComponent& that) = default;
            explicit MeshComponent(Ref<Mesh::Mesh> mesh) : mesh(mesh) {}

            Ref<Mesh::Mesh> mesh;

        };

        class LightComponent {

        public:
            LightComponent() = default;
            LightComponent(const LightComponent& that) = default;

        };

        class CameraComponent {

        public:
            CameraComponent() = default;
            CameraComponent(const CameraComponent& that) = default;

            bool main = true;
            


        };

        class AudioComponent {

        public:
            AudioComponent() = default;
            AudioComponent(const AudioComponent& that) = default;

        };

        class HierarchyComponent {

        public:
            HierarchyComponent() = default;
            HierarchyComponent(const HierarchyComponent& that) = default;

            bool root = false;
            std::vector<Entity> entities;

        };

    }

}

#endif