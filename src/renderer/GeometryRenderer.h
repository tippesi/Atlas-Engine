#ifndef GEOMETRYRENDERER_H
#define GEOMETRYRENDERER_H

#include "../System.h"
#include "../shader/ShaderBatch.h"
#include "IRenderer.h"

class GeometryRenderer : public IRenderer {

public:
	GeometryRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	static void InitShaderBatch();

	static void AddConfig(ShaderConfig* config);

	static void RemoveConfig(ShaderConfig* config);

	static string vertexPath;
	static string fragmentPath;

private:
	Uniform* diffuseMapUniform;
	Uniform* normalMapUniform;
	Uniform* specularMapUniform;
	Uniform* heightMapUniform;
	Uniform* modelMatrixUniform;
	Uniform* viewMatrixUniform;
	Uniform* projectionMatrixUniform;

	Uniform* diffuseColorUniform;
	Uniform* specularColorUniform;
	Uniform* ambientColorUniform;
	Uniform* specularHardnessUniform;
	Uniform* specularIntensityUniform;

	static ShaderBatch* shaderBatch;

};

#endif