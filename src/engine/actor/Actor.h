#ifndef AE_ACTOR_H
#define AE_ACTOR_H

#include "../System.h"
#include "../Camera.h"
#include "../volume/AABB.h"

namespace Atlas {

    namespace Actor {

        class Actor {

        public:
            Actor() = default;
            /**
             * Virtual destructor of the actor object.
             */
            virtual ~Actor() {}

            /**
             * Sets the transformation matrix of an actor relative to its parent
             * @param matrix A 4x4 matrix that contains the transformation of the actor.
             */
            inline virtual void SetMatrix(mat4 matrix) { this->matrix = matrix; matrixChanged = true; };

            /**
             * Returns the transformation matrix relative to its parent
             * @return A 4x4 matrix that contains the transformation of the actor.
             */
            inline mat4 GetMatrix() const { return matrix; }

            /**
             * Checks whether the transformation matrix of the actor has changed.
             * @return
             */
            inline bool HasMatrixChanged() const { return matrixChanged; }

            /**
             * Updates the axis-aligned bounding box and transformedMatrix according to it's
             * parent matrix. (Only if update is needed => parentUpdate && matrixChanged)
             * @param camera
             * @param deltaTime
             * @param parentTransform
             * @param parentUpdate
             */
            virtual void Update(Camera camera, float deltaTime, 
                mat4 parentTransform, bool parentUpdate) = 0;

            Volume::AABB aabb = Volume::AABB{ vec3{-1.0f}, vec3{1.0f} };
            mat4 globalMatrix = mat4{ 1.0f };
            mat4x3 inverseGlobalMatrix = mat4x3{ 1.0f };

            bool matrixChanged = true;
            bool visible = true;
            bool dontCull = false;

            std::string name;

        private:
            mat4 matrix = mat4(1.0f);

        };

    }

}


#endif
