#ifndef DECAL_H
#define DECAL_H

#include "System.h"

class Decal {

public:
    Decal(Texture* texture) : texture(texture) {}

    Texture* texture;
    mat4 matrix;

};


#endif