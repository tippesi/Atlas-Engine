#include "PrimitiveBatch.h"

namespace Atlas {

    namespace Renderer {

        PrimitiveBatch::PrimitiveBatch() {

            lineVertices = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);
            lineColors = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);

            triangleVertices = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);
            triangleColors = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);

        }

        void PrimitiveBatch::AddLine(vec3 from, vec3 to, vec3 fromColor, vec3 toColor) {

            lineVertexData.push_back(from);
            lineVertexData.push_back(to);

            lineColorData.push_back(fromColor);
            lineColorData.push_back(toColor);

            lineDataValid = false;

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

            if (!lineDataValid && GetLineCount() > 0) {

                lineVertices.SetSize(GetLineCount() * 2,
                    lineVertexData.data());
                lineColors.SetSize(GetLineCount() * 2,
                    lineColorData.data());

                lineDataValid = true;

            }

            if (!triangleDataValid && GetTriangleCount() > 0) {

                triangleVertices.SetSize(GetTriangleCount() * 3,
                    triangleVertexData.data());
                triangleColors.SetSize(GetTriangleCount() * 3,
                    triangleColorData.data());

                triangleDataValid = true;

            }

            UpdateVertexArrays();

        }

        void PrimitiveBatch::Clear() {

            lineVertexData.clear();
            lineColorData.clear();

        }

        void PrimitiveBatch::UpdateVertexArrays() {

            if (lineVertices.elementCount)
                lineVertexArray.AddComponent(0, lineVertices);
            if (lineColors.elementCount)
                lineVertexArray.AddComponent(1, lineColors);

            if (triangleVertices.elementCount)
                triangleVertexArray.AddComponent(0, triangleVertices);
            if (triangleColors.elementCount)
                triangleVertexArray.AddComponent(1, triangleColors);

        }

    }

}