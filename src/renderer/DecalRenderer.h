#ifndef AE_DECALRENDERER_H
#define AE_DECALRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class DecalRenderer : public IRenderer {

public:
    DecalRenderer();

    void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

    static std::string vertexPath;
    static std::string fragmentPath;

private:
    void GetUniforms();

    VertexArray vertexArray;

    Shader shader;

	Uniform* depthTexture;
    Uniform* decalTexture;
    Uniform* modelMatrix;
    Uniform* viewMatrix;
    Uniform* projectionMatrix;
    Uniform* inverseViewMatrix;
	Uniform* inverseProjectionMatrix;

	Uniform* color;

	Uniform* timeInMilliseconds;
	Uniform* animationLength;
	Uniform* rowCount;
	Uniform* columnCount;

};


#endif