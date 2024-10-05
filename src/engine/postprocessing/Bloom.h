#pragma once

#include "../System.h"

#include "resource/Resource.h"
#include "texture/Texture2D.h"

namespace Atlas::PostProcessing {

    class Bloom {

    public:
        Bloom() = default;

        bool enable = true;
        float strength = 0.01f;
        float threshold = 1.0f;

        float filterSize = 0.02f;
        uint32_t mipLevels = 6;

        ResourceHandle<Texture::Texture2D> dirtMap;

    };

}