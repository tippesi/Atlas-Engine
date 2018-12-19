#ifndef DECALRENDERER_H
#define DECALRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class DecalRenderer : public IRenderer {

public:
    DecalRenderer();

    void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

    static string vertexPath;
    static string fragmentPath;

private:
    void GetUniforms();

    VertexArray* vertexArray;

    Shader* shader;

	Uniform* depthTexture;
    Uniform* decalTexture;
    Uniform* modelMatrix;
    Uniform* viewMatrix;
    Uniform* projectionMatrix;
    Uniform* inverseViewMatrix;
	Uniform* inverseProjectionMatrix;

};


#endif