#pragma once

#include "../System.h"

#include "../texture/Cubemap.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Lighting {

        class EnvironmentProbe {

        public:
            EnvironmentProbe() = default;

            explicit EnvironmentProbe(const Texture::Cubemap& cubemap);

            explicit EnvironmentProbe(int32_t resolution, vec3 position = vec3(0.0f, 10.0f, 0.0f));

            void SetPosition(vec3 position);

            vec3 GetPosition();

            int32_t resolution;

            std::vector<mat4> viewMatrices;
            mat4 projectionMatrix;

            Texture::Cubemap cubemap;
            Texture::Cubemap depth;

            Texture::Cubemap filteredDiffuse;
            Texture::Cubemap filteredSpecular;

            bool update = true;

        private:
            vec3 position = vec3(0.0f, 10.0f, 0.0f);

        };

    }

}