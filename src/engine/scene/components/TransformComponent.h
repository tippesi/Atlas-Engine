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
                    mat4 matrix, bool isStatic = true) : isStatic(isStatic), entity(entity) {
                    Set(matrix);
                }

                void Set(const mat4& matrix);

                // Sets the world matrix immediately and derives the local matrix relative to the parent
                void SetGlobal(const mat4& matrix, const Entity& parentEntity);

                void ReconstructLocalMatrix(const Entity& parentEntity);

                // The local matrix ReconstructLocalMatrix would set, without changing the component
                mat4 GetReconstructedLocalMatrix(const Entity& parentEntity) const;

                bool IsStatic() const;

                void Translate(vec3 translation);
                void Rotate(vec3 rotation);
                void Scale(vec3 scale);

                Common::MatrixDecomposition Decompose() const;
                Common::MatrixDecomposition DecomposeGlobal() const;
                void Compose(Common::MatrixDecomposition composition);

                mat4 matrix = mat4{ 1.0f };

                bool isStatic = true;

                // Joint of the parent's animated mesh which the transform is relative to, -1 for the parent itself
                int32_t parentJoint = -1;

                mat4 globalMatrix = mat4{ 1.0f };
                mat4 lastGlobalMatrix = mat4{ 1.0f };
                mat4x3 inverseGlobalMatrix = mat4x3{ 1.0f };

            protected:
                void Update(const mat4& parentMatrix, bool parentChanged);

                Entity entity;

                bool changed = true;
                bool updated = false;
                bool wasStatic = true;
                bool initialized = false;
                bool lastGlobalMatrixSet = false;

            };

		}

	}

}