#ifndef AE_BVHRENDERER_H
#define AE_BVHRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "../volume/BVH.h"



namespace Atlas {

	namespace Renderer {

		class BVHRenderer : public Renderer {

		public:
			BVHRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			template<class T>
			void Render(Viewport* viewport, Camera* camera, Volume::BVH<T>* bvh, 
				bool blending = true, int32_t bvhLevel = -1);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			template<class T>
			size_t ProcessBVH(Volume::BVH<T>* bvh, int32_t bvhLevel);

			Buffer::VertexArray vertexArray;
			Buffer::IndexBuffer indexBuffer;
			Buffer::VertexBuffer vertexBuffer;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* modelMatrix;

			Shader::Uniform* color;

		};

		template<class T>
		void BVHRenderer::Render(Viewport* viewport, Camera* camera,
			Volume::BVH<T>* bvh, bool blending, int32_t bvhLevel) {

			if (blending) {
				glDisable(GL_CULL_FACE);
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			}

			shader.Bind();

			vertexArray.Bind();

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);
			modelMatrix->SetValue(mat4(1.0f));

			auto offset = ProcessBVH(bvh, bvhLevel);

			color->SetValue(vec4(1.0f, 0.0f, 0.0f, 1.0f));

			glDrawElements(AE_PRIMITIVE_TRIANGLES, offset, AE_UINT, 0);

			color->SetValue(vec4(0.0f, 1.0f, 0.0f, 1.0f));

			glDrawElements(AE_PRIMITIVE_TRIANGLES, indexBuffer.GetElementCount() - offset,
				AE_UINT, (const void*)(offset * indexBuffer.GetElementSize()));

			if (blending) {
				glEnable(GL_CULL_FACE);
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_TRUE);
			}

		}

		template<class T>
		size_t BVHRenderer::ProcessBVH(Volume::BVH<T>* bvh, int32_t bvhLevel) {

			auto bvhData = bvh->GetTree();

			int32_t index[] = {
				0, 1, 2, 2, 3, 0,
				1, 5, 6, 6, 2, 1,
				7, 6, 5, 5, 4, 7,
				4, 0, 3, 3, 7, 4,
				4, 5, 1, 1, 0, 4,
				3, 2, 6, 6, 7, 3 };

			std::vector<Volume::BVHNode<T>> leftChildren;
			std::vector<Volume::BVHNode<T>> rightChildren;

			if (bvhLevel < 0) {
				for (auto& node : bvhData) {
					if (node.isLeftChild)
						leftChildren.push_back(node);
					else
						rightChildren.push_back(node);
				}
			}
			else {
				for (auto& node : bvhData) {
					if (node.depth == bvhLevel) {
						if (node.isLeftChild)
							leftChildren.push_back(node);
						else
							rightChildren.push_back(node);
					}
				}
			}

			std::vector<vec3> leftBuffer;
			std::vector<vec3> rightBuffer;

			for (auto& node : leftChildren) {
				auto corners = node.aabb.GetCorners();
				for (auto ele : corners) {
					leftBuffer.push_back(ele);
				}
			}

			for (auto& node : rightChildren) {
				auto corners = node.aabb.GetCorners();
				for (auto ele : corners) {
					rightBuffer.push_back(ele);
				}
			}

			std::vector<uint32_t> indices;

			auto cubeCount = (uint32_t)(leftBuffer.size() + rightBuffer.size()) / 8;

			for (uint32_t i = 0; i < cubeCount; i++) {
				for (uint32_t j = 0; j < 36; j++) {
					indices.push_back(i * 8 + index[j]);
				}
			}

			std::vector<vec3> buffer;

			for (auto& ele : leftBuffer)
				buffer.push_back(ele);

			for (auto& ele : rightBuffer)
				buffer.push_back(ele);

			indexBuffer.SetSize(indices.size());
			vertexBuffer.SetSize(buffer.size());

			indexBuffer.SetData(indices.data(), 0, indices.size());
			vertexBuffer.SetData(buffer.data(), 0, buffer.size());
			//vertexBuffer.SetData(rightBuffer.data(), leftBuffer.size(), rightBuffer.size());

			AtlasLog("%d,%d,%d,%d,%d,%d", bvhLevel, cubeCount, (int32_t)leftChildren.size(),
				(int32_t)rightChildren.size(), (int32_t)leftBuffer.size() / 8 * 36, (int32_t)indices.size());

			return leftBuffer.size() / 8 * 36;

		}

	}

}


#endif