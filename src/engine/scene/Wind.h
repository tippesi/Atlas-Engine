#pragma once

#include "../System.h"
#include "../texture/Texture2D.h"

namespace Atlas {

    namespace Scene {

        class Wind {

        public:
            Wind();

            vec2 direction = vec2(1.0f);
            float speed = 10.0f;

            Texture::Texture2D noiseMap;

        };

    }

}