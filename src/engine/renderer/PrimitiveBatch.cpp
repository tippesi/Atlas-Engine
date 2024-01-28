#include "PrimitiveBatch.h"

namespace Atlas {

    namespace Renderer {

        PrimitiveBatch::PrimitiveBatch() {

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

        void PrimitiveBatch::AddLine(vec3 from, vec3 to, vec3 fromColor, vec3 toColor) {

            /*
            lineVertexData.push_back(from);
            lineVertexData.push_back(to);

            lineColorData.push_back(fromColor);
            lineColorData.push_back(toColor);

            lineDataValid = false;
             */

        }

        size_t PrimitiveBatch::GetLineCount() const {

            return lineVertexData.size() / 2;

        }

        void PrimitiveBatch::SetLineWidth(float width) {

            lineWidth = width;

        }

        float PrimitiveBatch::GetLineWidth() const {

            return lineWidth;

        }

        void PrimitiveBatch::AddTriangle(vec3 v0, vec3 v1, vec3 v2, vec3 v0Color,
            vec3 v1Color, vec3 v2Color) {

            triangleVertexData.push_back(v0);
            triangleVertexData.push_back(v1);
            triangleVertexData.push_back(v2);

            triangleColorData.push_back(v0Color);
            triangleColorData.push_back(v1Color);
            triangleColorData.push_back(v2Color);

            triangleDataValid = false;

        }

        size_t PrimitiveBatch::GetTriangleCount() const {

            return triangleVertexData.size() / 3;

        }

        void PrimitiveBatch::TransferData() {

            if (!lineDataValid) {

                lineVertices.SetSize(GetLineCount() * 2,
                    lineVertexData.data());
                lineColors.SetSize(GetLineCount() * 2,
                    lineColorData.data());

                lineDataValid = true;

            }

            if (!triangleDataValid) {

                triangleVertices.SetSize(GetTriangleCount() * 3,
                    triangleVertexData.data());
                triangleColors.SetSize(GetTriangleCount() * 3,
                    triangleColorData.data());

                triangleDataValid = true;

            }

        }

        void PrimitiveBatch::BindLineBuffer() {

            // lineVertexArray.Bind();

        }

        void PrimitiveBatch::BindTriangleBuffer() {

            // triangleVertexArray.Bind();

        }

        void PrimitiveBatch::Clear() {

            lineVertexData.clear();
            lineColorData.clear();

        }

    }

}