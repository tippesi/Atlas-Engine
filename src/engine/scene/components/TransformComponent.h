#pragma once

#include "../../System.h"

#include "../../volume/AABB.h"
#include "../../common/MatrixDecomposition.h"

namespace Atlas {

	namespace Scene {

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

                void Set(const glm::mat4& matrix);

                void Translate(glm::vec3 translation);
                void Rotate(glm::vec3 rotation);
                void Scale(glm::vec3 scale);

                Common::MatrixDecomposition Decompose() const;
                void Compose(Common::MatrixDecomposition composition);

                glm::mat4 matrix = mat4{ 1.0f };

                mat4 globalMatrix = mat4{ 1.0f };
                mat4 lastGlobalMatrix = mat4{ 1.0f };
                mat4x3 inverseGlobalMatrix = mat4x3{ 1.0f };

            protected:
                void Update(const TransformComponent& parentTransform, bool parentChanged);

                bool changed = true;
                bool updated = false;

                bool isStatic = true;

            };

		}

	}

}