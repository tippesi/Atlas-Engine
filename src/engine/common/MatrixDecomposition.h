#pragma once

#include "../System.h"

namespace Atlas {

    namespace Common {
        
        class MatrixDecomposition {

        public:
            MatrixDecomposition() = default;

            explicit MatrixDecomposition(const mat4& matrix);

            void Decompose(const mat4& matrix);

            mat4 Compose();

            vec3 translation = vec3(0.0f);
            vec3 rotation = vec3(0.0f);
            vec3 scale = vec3(1.0f);

            quat quat;

        };

    }

}