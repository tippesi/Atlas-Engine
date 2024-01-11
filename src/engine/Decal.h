#pragma once

#include "System.h"
#include "texture/Texture2D.h"

namespace Atlas {

    class Decal {

    public:
        Decal() = default;

        explicit Decal(Texture::Texture2D *texture, float rowCount = 1.0f, float columnCount = 1.0f, float animationLength = 1.0f)
            : texture(texture), rowCount(rowCount), columnCount(columnCount), animationLength(animationLength) {}

        Texture::Texture2D *texture = nullptr;

        float rowCount = 1.0f;
        float columnCount = 1.0f;
        float animationLength = 1.0f;

    };

}