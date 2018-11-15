#ifndef TERRAINRENDERER_H
#define TERRAINRENDERER_H

#include "../System.h§

class TerrainRenderer {

public:
    TerrainRenderer(const char* vertexSource, const char* fragmentSource);

private:
    Shader* nearShader;
    Shader* middleShader;
    Shader* farShader;

};


#endif
