#ifndef TERRAINRENDERER_H
#define TERRAINRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class TerrainRenderer : public IRenderer {

public:
    TerrainRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	void GetUniforms();

	static string vertexPath;
	static string tessControlPath;
	static string tessEvalPath;
	static string geometryPath;
	static string fragmentPath;

private:
    Shader* nearShader;
    Shader* middleShader;
    Shader* farShader;

	Uniform* heightField;
	Uniform* normalMap;
	Uniform* diffuseMap;
	Uniform* displacementMap;

	Uniform* heightScale;
	Uniform* offset;
	Uniform* tileScale;
	Uniform* modelMatrix;
	Uniform* viewMatrix;
	Uniform* projectionMatrix;
	Uniform* cameraLocation;
	Uniform* nodeSideLength;
	Uniform* nodeLocation;

	Uniform* patchOffsets;
	Uniform* patchOffsetsScale;

	Uniform* tessellationFactor;
	Uniform* tessellationSlope;
	Uniform* tessellationShift;
	Uniform* maxTessellationLevel;

	Uniform* displacementScale;
	Uniform* displacementDistance;

};


#endif
