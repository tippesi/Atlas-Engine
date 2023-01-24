#ifndef AE_TEXTURERENDERER_H
#define AE_TEXTURERENDERER_H

#include "Renderer.h"

#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Texture3D.h"

namespace Atlas {

	namespace Renderer {

		class TextureRenderer : public Renderer {

		public:
			TextureRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target,
                Camera* camera, Scene::Scene* scene) final {};

			void RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture2D* texture,
                float x, float y, float width, float height, bool alphaBlending = false, bool invert = false);

			void RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture2D* texture,
                float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
                bool alphaBlending = false, bool invert = false);

			void RenderTexture2DArray(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture2DArray* texture,
                int32_t depth, float x, float y, float width, float height, bool alphaBlending = false, bool invert = false);

			void RenderTexture2DArray(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture2DArray* texture,
                int32_t depth, float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
                bool alphaBlending = false, bool invert = false);

			void RenderTexture3D(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture3D* texture,
                float depth, float x, float y, float width, float height, bool alphaBlending = false, bool invert = false);

			void RenderTexture3D(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture3D* texture,
                float depth, float x, float y, float width, float height,
				vec4 clipArea, vec4 blendArea, bool alphaBlending = false, bool invert = false);

		private:
            struct alignas(16) PushConstants {
                mat4 pMatrix;
                vec4 blendArea;
                vec4 clipArea;
                vec2 offset;
                vec2 scale;
                int invert;
            };

            PipelineConfig GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer, const std::string& macro);

			Buffer::VertexArray vertexArray;

            /*
			OldShader::OldShader texture2DShader;
			OldShader::OldShader texture2DArrayShader;
			OldShader::OldShader texture3DShader;
            */

		};

	}

}

#endif