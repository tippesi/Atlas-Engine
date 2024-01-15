#pragma once

#include "../../System.h"

#include "../../volume/AABB.h"
#include "../../common/MatrixDecomposition.h"

namespace Atlas {

	namespace NewScene {

        class Scene;
        class SpacePartitioning;

		namespace Components {

            class HierarchyComponent;

            class TransformComponent {

                friend Scene;
                friend SpacePartitioning;
                friend HierarchyComponent;

            public:
                TransformComponent() = default;
                TransformComponent(const TransformComponent& that) = default;
                explicit TransformComponent(mat4 matrix, bool isStatic = true) : matrix(matrix), isStatic(isStatic) {}

                void Translate(glm::vec3 translation);
                void Rotate(glm::vec3 rotation);
                void Scale(glm::vec3 scale);

                Common::MatrixDecomposition Decompose() const;
                void Compose(Common::MatrixDecomposition composition);

                glm::mat4 matrix;

                const bool isStatic = true;

            protected:
                void Update(const TransformComponent& parentTransform, bool parentChanged);

                Volume::AABB aabb = Volume::AABB{ vec3{-1.0f}, vec3{1.0f} };
                mat4 globalMatrix = mat4{ 1.0f };
                mat4x3 inverseGlobalMatrix = mat4x3{ 1.0f };

                bool changed = true;

            };

		}

	}

}