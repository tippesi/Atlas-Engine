#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../shader/ShaderBatch.h"
#include "IRenderer.h"

#include <mutex>

class GeometryRenderer : public IRenderer {

public:
	GeometryRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static void InitShaderBatch();

	static void AddConfig(ShaderConfig* config);

	static void RemoveConfig(ShaderConfig* config);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	Uniform* arrayMapUniform;
	Uniform* diffuseMapUniform;
	Uniform* normalMapUniform;
	Uniform* specularMapUniform;
	Uniform* heightMapUniform;

	Uniform* diffuseMapIndexUniform;
	Uniform* normalMapIndexUniform;
	Uniform* specularMapIndexUniform;
	Uniform* heightMapIndexUniform;

	Uniform* modelMatrixUniform;
	Uniform* viewMatrixUniform;
	Uniform* projectionMatrixUniform;

	Uniform* diffuseColorUniform;
	Uniform* specularColorUniform;
	Uniform* ambientColorUniform;
	Uniform* specularHardnessUniform;
	Uniform* specularIntensityUniform;

	static ShaderBatch shaderBatch;
	static std::mutex shaderBatchMutex;

};

#endif