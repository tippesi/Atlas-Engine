#pragma once

#include "../../System.h"

#include "../../volume/AABB.h"
#include "../../common/MatrixDecomposition.h"

#include "../Entity.h"

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
                TransformComponent(Scene* scene, Entity entity) : entity(entity) {}
                TransformComponent(Scene* scene, Entity entity, const TransformComponent& that);
                explicit TransformComponent(Scene* scene, Entity entity, 
                    mat4 matrix, bool isStatic = true) : matrix(matrix), isStatic(isStatic), entity(entity) {}

                void Set(const glm::mat4& matrix);

                void ReconstructLocalMatrix(const Entity& parentEntity);

                bool IsStatic() const;

                void Translate(glm::vec3 translation);
                void Rotate(glm::vec3 rotation);
                void Scale(glm::vec3 scale);

                Common::MatrixDecomposition Decompose() const;
                Common::MatrixDecomposition DecomposeGlobal() const;
                void Compose(Common::MatrixDecomposition composition);

                glm::mat4 matrix = mat4{ 1.0f };

                bool isStatic = true;

                mat4 globalMatrix = mat4{ 1.0f };
                mat4 lastGlobalMatrix = mat4{ 1.0f };
                mat4x3 inverseGlobalMatrix = mat4x3{ 1.0f };

            protected:
                void Update(const TransformComponent& parentTransform, bool parentChanged);

                Entity entity;

                bool changed = true;
                bool updated = false;
                bool wasStatic = true;

            };

		}

	}

}