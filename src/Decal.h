#ifndef DECAL_H
#define DECAL_H

#include "System.h"
#include "texture/Texture2D.h"

class Decal {

public:
    Decal(Texture2D* texture, float rowCount = 1.0f, float columnCount = 1.0f, float animationLength = 1.0f)
        : texture(texture), rowCount(rowCount), columnCount(columnCount), animationLength(animationLength) {}

    Texture2D* texture;

    float rowCount;
    float columnCount;
    float animationLength;

    mat4 matrix;

};


#endif