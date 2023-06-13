#ifndef AE_MATRIXDECOMPOSITION_H
#define AE_MATRIXDECOMPOSITION_H

#include "../System.h"

namespace Atlas {

    namespace Common {
        
        class MatrixDecomposition {

        public:
            MatrixDecomposition() = default;

            explicit MatrixDecomposition(mat4 matrix);

            void Decompose(mat4 matrix);

            mat4 Compose();

            vec3 translation = vec3(0.0f);
            vec3 rotation = vec3(0.0f);
            vec3 scale = vec3(1.0f);

        };

    }

}



#endif