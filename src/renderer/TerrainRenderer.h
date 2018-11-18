#ifndef TERRAINRENDERER_H
#define TERRAINRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class TerrainRenderer : public IRenderer {

public:
    TerrainRenderer(const char* vertexSource, const char* tessControlSource, const char* tessEvalSource, 
		const char* geometrySource, const char* fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	void GetUniforms();

private:
    Shader* nearShader;
    Shader* middleShader;
    Shader* farShader;

	Uniform* heightField;
	Uniform* heightScale;
	Uniform* offset;
	Uniform* scale;
	Uniform* modelMatrix;
	Uniform* viewMatrix;
	Uniform* projectionMatrix;
	Uniform* nodeSideLength;
	Uniform* nodeLocation;

};


#endif
