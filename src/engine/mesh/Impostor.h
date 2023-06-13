#ifndef AE_IMPOSTOR_H
#define AE_IMPOSTOR_H

#include "../System.h"
#include "../volume/AABB.h"
#include "../texture/Texture2DArray.h"
#include "../buffer/Buffer.h"

namespace Atlas {

    namespace Mesh {

        class Impostor {

        public:
            Impostor() = default;

            Impostor(const Impostor& that);

            Impostor(int32_t views, int32_t resolution);

            Impostor& operator=(const Impostor& that);

            void FillViewPlaneBuffer(std::vector<vec3> rightVectors, std::vector<vec3> upVectors);

            Texture::Texture2DArray baseColorTexture;
            Texture::Texture2DArray roughnessMetalnessAoTexture;
            Texture::Texture2DArray normalTexture;

            Buffer::Buffer viewPlaneBuffer;

            vec3 center = vec3(0.0f);
            float radius = 1.0f;

            int32_t views = 1;
            int32_t resolution = 64;

            float cutoff = 0.7f;

            bool interpolation = false;
            vec3 transmissiveColor = vec3(0.0f);

        private:
            struct ViewPlane {
                vec4 right;
                vec4 up;
            };

        };

    }

}

#endif