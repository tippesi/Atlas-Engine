#ifndef DECAL_H
#define DECAL_H

#include "System.h"
#include "texture/Texture2D.h"

class Decal {

public:
    Decal(Texture2D* texture, float rowCount, float columnCount, float animationLength)
        : texture(texture), rowCount(rowCount), columnCount(columnCount), animationLength(animationLength) {}

    Texture2D* texture;

    float rowCount;
    float columnCount;
    float animationLength;

    mat4 matrix;

};


#endif