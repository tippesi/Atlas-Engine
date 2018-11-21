#ifndef TERRAINRENDERER_H
#define TERRAINRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class TerrainRenderer : public IRenderer {

public:
    TerrainRenderer(string vertexSource, string tessControlSource, string tessEvalSource,
		string geometrySource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	void GetUniforms();

private:
    Shader* nearShader;
    Shader* middleShader;
    Shader* farShader;

	Uniform* heightField;
	Uniform* normalMap;
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
