#include "RenderBatch.h"

namespace Atlas {

	namespace Renderer {

		RenderBatch::RenderBatch() {

            /*
			lineVertices = Atlas::Buffer::VertexBuffer(GL_FLOAT,
				3, sizeof(vec3), 0, nullptr, AE_BUFFER_DYNAMIC_STORAGE);
			lineColors = Atlas::Buffer::VertexBuffer(GL_FLOAT,
				3, sizeof(vec3), 0, nullptr, AE_BUFFER_DYNAMIC_STORAGE);

			lineVertexArray.Bind();

			lineVertexArray.AddComponent(0, &lineVertices);
			lineVertexArray.AddComponent(1, &lineColors);

			lineVertexArray.Unbind();

			triangleVertices = Atlas::Buffer::VertexBuffer(GL_FLOAT,
				3, sizeof(vec3), 0, nullptr, AE_BUFFER_DYNAMIC_STORAGE);
			triangleColors = Atlas::Buffer::VertexBuffer(GL_FLOAT,
				3, sizeof(vec3), 0, nullptr, AE_BUFFER_DYNAMIC_STORAGE);

			triangleVertexArray.Bind();

			triangleVertexArray.AddComponent(0, &triangleVertices);
			triangleVertexArray.AddComponent(1, &triangleColors);

			triangleVertexArray.Unbind();
             */

		}

		void RenderBatch::AddLine(vec3 from, vec3 to, vec3 fromColor, vec3 toColor) {

            /*
			lineVertexData.push_back(from);
			lineVertexData.push_back(to);

			lineColorData.push_back(fromColor);
			lineColorData.push_back(toColor);

			lineDataValid = false;
             */

		}

		size_t RenderBatch::GetLineCount() const {

			return lineVertexData.size() / 2;

		}

		void RenderBatch::SetLineWidth(float width) {

			lineWidth = width;

		}

		float RenderBatch::GetLineWidth() const {

			return lineWidth;

		}

		void RenderBatch::AddTriangle(vec3 v0, vec3 v1, vec3 v2, vec3 v0Color,
			vec3 v1Color, vec3 v2Color) {

			triangleVertexData.push_back(v0);
			triangleVertexData.push_back(v1);
			triangleVertexData.push_back(v2);

			triangleColorData.push_back(v0Color);
			triangleColorData.push_back(v1Color);
			triangleColorData.push_back(v2Color);

			triangleDataValid = false;

		}

		size_t RenderBatch::GetTriangleCount() const {

			return triangleVertexData.size() / 3;

		}

		void RenderBatch::TransferData() {

			if (!lineDataValid) {

				lineVertices.SetSize(GetLineCount() * 2,
					lineVertexData.data());
				lineColors.SetSize(GetLineCount() * 2,
					lineColorData.data());

				lineVertexArray.UpdateComponents();

				lineDataValid = true;

			}

			if (!triangleDataValid) {

				triangleVertices.SetSize(GetTriangleCount() * 3,
					triangleVertexData.data());
				triangleColors.SetSize(GetTriangleCount() * 3,
					triangleColorData.data());

				lineVertexArray.UpdateComponents();

				triangleDataValid = true;

			}

		}

		void RenderBatch::BindLineBuffer() {

			// lineVertexArray.Bind();

		}

		void RenderBatch::BindTriangleBuffer() {

			// triangleVertexArray.Bind();

		}

		void RenderBatch::Clear() {

			lineVertexData.clear();
			lineColorData.clear();

		}

	}

}