#pragma once

#include "System.h"
#include "buffer/VertexArray.h"
#include "buffer/VertexBuffer.h"
#include "volume/AABB.h"
#include "volume/Frustum.h"
#include "volume/Rectangle.h"

#include <vector>

namespace Atlas {

    namespace Renderer {

        class PrimitiveBatch {

        public:
            PrimitiveBatch();

            void AddLine(vec3 from, vec3 to, vec3 fromColor = vec3(1.0f),
                vec3 toColor = vec3(1.0f));

            void AddLineAABB(Volume::AABB aabb, vec3 color);

            void AddLineFrustum(const Volume::Frustum& frustum, vec3 color);

            void AddLineSphere(vec3 center, float radius, vec3 color);

            void AddLineRectangle(const Volume::Rectangle& rectangle, vec3 color);

            // Three axis aligned lines through the position
            void AddLineCross(vec3 position, float size, vec3 color);

            // Upright cylinder standing on the base point, e.g. a stand-in for a character
            void AddLineCylinder(vec3 base, float height, float radius, vec3 color);

            // The world axes in red (x), green (y) and blue (z)
            void AddLineAxes(vec3 position, float length);

            size_t GetLineCount() const;

            void SetLineWidth(float width);

            float GetLineWidth() const;

            void AddTriangle(vec3 v0, vec3 v1, vec3 v2,
                vec3 v0Color, vec3 v1Color, vec3 v2Color);

            size_t GetTriangleCount() const;

            void TransferData();

            void Clear();

            bool testDepth = true;

            Buffer::VertexArray lineVertexArray;
            Buffer::VertexArray triangleVertexArray;

        private:
            void UpdateVertexArrays();

            // Line data
            Buffer::VertexBuffer lineVertices;
            Buffer::VertexBuffer lineColors;

            std::vector<vec3> lineVertexData;
            std::vector<vec3> lineColorData;

            float lineWidth = 1.0f;

            bool lineDataValid = true;

            // Triangle data
            Buffer::VertexBuffer triangleVertices;
            Buffer::VertexBuffer triangleColors;

            std::vector<vec3> triangleVertexData;
            std::vector<vec3> triangleColorData;

            bool triangleDataValid = true;

        };

    }

}