#ifndef AE_RENDERBATCH_H
#define AE_RENDERBATCH_H

#include "System.h"
#include "buffer/VertexArray.h"
#include "buffer/VertexBuffer.h"

#include <vector>

namespace Atlas {

	namespace Renderer {

		class RenderBatch {

		public:
			RenderBatch();

			void AddLine(vec3 from, vec3 to, vec3 fromColor = vec3(1.0f),
				vec3 toColor = vec3(1.0f));

			size_t GetLineCount() const;

			void SetLineWidth(float width);

			float GetLineWidth() const;

			void AddTriangle(vec3 v0, vec3 v1, vec3 v2,
				vec3 v0Color, vec3 v1Color, vec3 v2Color);

			size_t GetTriangleCount() const;

			void TransferData();

			void BindLineBuffer();

			void BindTriangleBuffer();

			void Clear();

		private:
			// Line data
			OldBuffer::VertexArray lineVertexArray;
			OldBuffer::VertexBuffer lineVertices;
			OldBuffer::VertexBuffer lineColors;

			std::vector<vec3> lineVertexData;
			std::vector<vec3> lineColorData;

			float lineWidth = 1.0f;

			bool lineDataValid = true;

			// Triangle data
			OldBuffer::VertexArray triangleVertexArray;
			OldBuffer::VertexBuffer triangleVertices;
			OldBuffer::VertexBuffer triangleColors;

			std::vector<vec3> triangleVertexData;
			std::vector<vec3> triangleColorData;

			bool triangleDataValid = true;

		};

	}

}

#endif