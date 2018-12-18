#ifndef DECAL_H
#define DECAL_H

#include "System.h"
#include "texture/Texture2D.h"

class Decal {

public:
    Decal(Texture2D* texture) : texture(texture) {}

    Texture2D* texture;
    mat4 matrix;

};


#endif